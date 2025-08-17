#include "CircularMaze.h"
#include <Arduino.h>
#include <math.h>
#include <vector>


CircularMaze::CircularMaze(int rings, int sectors, int spacing) {
    NUM_RINGS = rings;
    SECTORS_PER_RING = sectors;
    RING_SPACING = spacing;
    wall_buffer_idx = 0;
    // Resize the 2D vectors to match the new dimensions
    radial_walls.resize(NUM_RINGS, std::vector<bool>(SECTORS_PER_RING));
    circular_walls.resize(NUM_RINGS, std::vector<bool>(SECTORS_PER_RING));
    visited_circular.resize(NUM_RINGS, std::vector<bool>(SECTORS_PER_RING));

    // Resize the LVGL point buffer
    int max_walls = (NUM_RINGS * SECTORS_PER_RING) * 2 + 1;
    point_buffer.resize(max_walls);
}

void CircularMaze::generate() {
    for (int r = 0; r < NUM_RINGS; r++) {
        for (int s = 0; s < SECTORS_PER_RING; s++) {
            radial_walls[r][s] = true;
            circular_walls[r][s] = true;
            visited_circular[r][s] = false;
        }
    }
    // Set the centermost area of the maze as already visited so the inner area is fully carved out
    for (int s = 0; s < SECTORS_PER_RING; ++s) {
        visited_circular[0][s] = true; 
    }

    // Create an exit on the outer perimeter
    placeExitAndSpawn();

    carve(NUM_RINGS - 1, exit_sector);

}

void CircularMaze::placeExitAndSpawn() {
    // EXIT on outer perimeter at a random sector (no wall removal)
    exit_sector = random(0, SECTORS_PER_RING);

    const float step = 2.0f * M_PI / SECTORS_PER_RING;
    float exit_angle_mid = (exit_sector + 0.5f) * step;      // center of sector
    float exit_radius    = (NUM_RINGS - 0.5) * RING_SPACING;         // perimeter radius

    exit_px = {
        (lv_coord_t)(CENTER_X + cosf(exit_angle_mid) * exit_radius),
        (lv_coord_t)(CENTER_Y + sinf(exit_angle_mid) * exit_radius)
    };

    // SPAWN at diametrically opposite angle, at a safe "between-arcs" radius
    int opposite_sector = (exit_sector + SECTORS_PER_RING / 2) % SECTORS_PER_RING;
    float spawn_angle   = (opposite_sector) * step;

    ball_spawn_px = {
        (lv_coord_t)(CENTER_X + cosf(spawn_angle) * exit_radius),
        (lv_coord_t)(CENTER_Y + sinf(spawn_angle) * exit_radius)
    };
}

void CircularMaze::draw(lv_obj_t* parent, bool animate) {
    wall_buffer_idx = 0; // Reset buffer index each time we redraw

    // LVGL One-time style init for wall lines
    static lv_style_t style_wall_circular;
    static bool s_style_init = false;
    if (!s_style_init) {
        lv_style_init(&style_wall_circular);
        lv_style_set_line_width(&style_wall_circular, 2);
        lv_style_set_line_color(&style_wall_circular, lv_color_white());
        lv_style_set_line_rounded(&style_wall_circular, true);
        s_style_init = true;
    }
    const float angle_step = 2 * M_PI / SECTORS_PER_RING;

    // Draw Radial Walls (Spokes) — one spoke segment per annulus
    for (int ring = 2; ring < NUM_RINGS; ++ring) {
        float r1 = ring * RING_SPACING;           // inner arc of this annulus
        float r2 = (ring + 1) * RING_SPACING;     // outer arc of this annulus
        for (int s = 0; s < SECTORS_PER_RING; ++s) {
            if (!radial_walls[ring - 1][s]) continue;  // <-- annulus stored at ring-1!
            float angle = s * angle_step;

            if (wall_buffer_idx < point_buffer.size()) {
                point_buffer[wall_buffer_idx][0] = {
                    (lv_coord_t)(CENTER_X + cosf(angle) * r1),
                    (lv_coord_t)(CENTER_Y + sinf(angle) * r1)
                };
                point_buffer[wall_buffer_idx][1] = {
                    (lv_coord_t)(CENTER_X + cosf(angle) * r2),
                    (lv_coord_t)(CENTER_Y + sinf(angle) * r2)
                };
                lv_obj_t *wall = lv_line_create(parent);
                lv_line_set_points(wall, point_buffer[wall_buffer_idx].data(), 2);
                lv_obj_add_style(wall, &style_wall_circular, 0);
                wall_buffer_idx++;
                if (animate) lv_timer_handler();
            }
        }
    }
    
    // Draw Circular Walls (Arcs)
    for (int r = 1; r < NUM_RINGS; r++) { // no need to draw the first ring since theere are no walls there
        for (int s = 0; s < SECTORS_PER_RING; s++) {
            if (circular_walls[r][s]) {
                if (wall_buffer_idx < point_buffer.size()) {
                    float radius = (r + 1) * RING_SPACING;
                    float start_angle = s * angle_step;
                    
                    for(int i = 0; i <= POINTS_PER_ARC; i++) {
                        float current_angle = start_angle + (float)i * (angle_step / POINTS_PER_ARC);
                        point_buffer[wall_buffer_idx][i].x = (lv_coord_t)(CENTER_X + cos(current_angle) * radius);
                        point_buffer[wall_buffer_idx][i].y = (lv_coord_t)(CENTER_Y + sin(current_angle) * radius);
                    }

                    lv_obj_t *arc_wall = lv_line_create(parent);
                    lv_line_set_points(arc_wall, point_buffer[wall_buffer_idx].data(), POINTS_PER_ARC + 1);
                    lv_obj_add_style(arc_wall, &style_wall_circular, 0);
                    wall_buffer_idx++;

                    if (animate) lv_timer_handler();
                }
            }
        }
    }

    // Draw exit
    lv_obj_t* exitObj = lv_obj_create(parent);
    int dot = max(2, RING_SPACING - 2);
    lv_obj_set_size(exitObj, dot, dot);
    lv_obj_set_pos(exitObj, exit_px.x - dot/2, exit_px.y - dot/2);
    lv_obj_set_style_bg_color(exitObj, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_border_width(exitObj, 0, 0);
    lv_obj_set_style_radius(exitObj, 0, 0);  // square, not circle

    // Final actual draw to screen
    lv_timer_handler();
}

// Very similar to the rectangular maze carve
void CircularMaze::carve(int ring, int sector) {
    visited_circular[ring][sector] = true;

    // Fisher-Yates shuffle 1-liner
    int dirs[4] = {0,1,2,3}; // 0=Out, 1=In, 2=CW, 3=CCW
    for (int i = 3; i > 0; --i) { int j = random(i+1); int t = dirs[i]; dirs[i]=dirs[j]; dirs[j]=t; }

    for (int k = 0; k < 4; ++k) {
        int dir = dirs[k];
        int nr = ring, ns = sector;

        if (dir == 0) nr = ring + 1;                           // Out
        else if (dir == 1) nr = ring - 1;                      // In
        else if (dir == 2) ns = (sector + 1) % SECTORS_PER_RING;   // CW
        else               ns = (sector - 1 + SECTORS_PER_RING) % SECTORS_PER_RING; // CCW

        // Playable rings are 1..NUM_RINGS-1 (exclude the hub at 0)
        if (nr < 1 || nr >= NUM_RINGS) continue;
        if (visited_circular[nr][ns]) continue;

        // Knock down the wall BETWEEN (ring,sector) and (nr,ns)
        if (dir == 0) {                                  // Out -> clear outer arc at (ring+1)*spacing
            circular_walls[ring][sector] = false;
        } else if (dir == 1) {                           // In -> clear inner arc at ring*spacing
            circular_walls[ring - 1][sector] = false;
        } else if (dir == 2) {                           // CW -> clear spoke at (sector+1)*step across THIS annulus
            radial_walls[ring - 1][(sector + 1) % SECTORS_PER_RING] = false;
        } else {                                         // CCW -> clear spoke at sector*step across THIS annulus
            radial_walls[ring - 1][sector] = false;
        }

        carve(nr, ns);
    }
}


// Note:
// Arc normals are radial thus reflect radial component; spoke normals are tangential thus reflect tangential component.
void CircularMaze::handleCollisions(Ball &ball) {
    float cx = ball.getX();
    float cy = ball.getY();
    const float br = ball.getRadius();

    // polar coords from ball x and y 
    float dx = cx - CENTER_X;
    float dy = cy - CENTER_Y;
    float r  = sqrtf(dx*dx + dy*dy); // distance from maze center to ball center
    if (r <= 1e-6f) return;

    float a = atan2f(dy, dx); // angle in radians
    if (a < 0) a += 2.0f * M_PI; // shifted to [0,  2*Pi]

    //computer current ring and sector ball is currently in
    const float step = (2.0f * M_PI) / SECTORS_PER_RING;
    int ring = (int)floorf(r / RING_SPACING);          // polar "ring" index (0..)
    if (ring < 0) ring = 0;
    if (ring > NUM_RINGS - 1) ring = NUM_RINGS - 1;

    int sector = (int)floorf(a / step); // sector ball is in
    if (sector < 0) sector = 0;
    if (sector > SECTORS_PER_RING - 1) sector = SECTORS_PER_RING - 1;

    // basis (FLOATS, not lv_point_t)
    float urx = cosf(a),  ury = sinf(a);               // radial unit
    float utx = -sinf(a), uty = cosf(a);               // tangential unit

    auto dot = [](float x1,float y1,float x2,float y2){ return x1*x2 + y1*y2; };

    bool collided = false;
    float vx = ball.getVelocityX();
    float vy = ball.getVelocityY();

    // ---------- ARCS ----------
    // INNER arc of annulus 'ring' lives at circular_walls[ring-1][*] at radius = ring*spacing
    if (ring > 1 && circular_walls[ring - 1][sector]) {
        float arcR = ring * RING_SPACING;
        float pen  = (arcR + br) - r;   // >0 if ball center is too far inward
        // if arc is penetrated
        if (pen > 0.0f) {
            float new_r = arcR + br; // move ball outward from arc
            cx = CENTER_X + new_r * urx;
            cy = CENTER_Y + new_r * ury;

            float v_r = dot(vx, vy, urx, ury); // radial speed
            float v_t = dot(vx, vy, utx, uty); // tangential speed
            v_r = -0.25f * v_r;           // damped reflection
            vx  = v_r*urx + v_t*utx;      // calculate new radially dampened velocity 
            vy  = v_r*ury + v_t*uty;

            //r = new_r;
            collided = true;
        }
    }

  // OUTER arc of annulus 'ring' lives at circular_walls[ring][*] at radius = (ring+1)*spacing
  if (ring > 0 && ring < NUM_RINGS && circular_walls[ring][sector]) {
    float arcR = (ring + 1) * RING_SPACING;
    float pen  = r + br - arcR;     // >0 if ball center too far outward
    if (pen > 0.0f) {
      float new_r = arcR - br;
      cx = CENTER_X + new_r * urx;
      cy = CENTER_Y + new_r * ury;

      float v_r = dot(vx, vy, urx, ury);
      float v_t = dot(vx, vy, utx, uty);
      v_r = -0.25f * v_r;
      vx  = v_r*urx + v_t*utx;
      vy  = v_r*ury + v_t*uty;

      //r = new_r;
      collided = true;
    }
  }

  // ---------- SPOKES ----------
  // spokes across THIS annulus exist only for ring >= 2 and live in radial_walls[ring-1][*]
  if (ring > 1) {
    float rInner = ring * RING_SPACING;
    float rOuter = (ring + 1) * RING_SPACING;

    auto resolveSpoke = [&](int spokeS) {
      if (!radial_walls[ring - 1][spokeS]) return;
      float spokeA = spokeS * step; // spkoe angle
      float dA = a - spokeA; 
      // perpendicular distance to the spoke line through origin
      float perp = fabsf(r * sinf(dA));
      bool insideSpan = (r >= rInner - br) && (r <= rOuter + br);
      if (!insideSpan) return;
      if (perp <= br) { // if ball intersects spoke
        // push along tangent to achieve perp == br
        float sign = (sinf(dA) >= 0.f) ? 1.f : -1.f; // Are we hitting the CW or CCW spoke
        float need = (br - perp);
        cx += sign * utx * need;
        cy += sign * uty * need;

        // recompute basis
        dx = cx - CENTER_X;
        dy = cy - CENTER_Y;
        r = sqrtf(dx*dx + dy*dy);
        a  = atan2f(dy, dx);
        if (a < 0) a += 2.0f*M_PI;
        urx = cosf(a);  ury = sinf(a);
        utx = -sinf(a); uty = cosf(a);

        float v_r = dot(vx, vy, urx, ury);
        float v_t = dot(vx, vy, utx, uty);
        v_t = -0.25f * v_t;  // reflect across tangent (spoke normal)
        vx  = v_r*urx + v_t*utx;
        vy  = v_r*ury + v_t*uty;

        collided = true;
      }
    };

    // left boundary at angle = sector*step, right boundary at (sector+1)*step
    resolveSpoke(sector);
    resolveSpoke((sector + 1) % SECTORS_PER_RING);
  }

  if (collided) {
    ball.setX(cx); ball.setY(cy);
    ball.setVelocityX(vx); ball.setVelocityY(vy);
  }
}


// Same as in rectnagular maze but handle collisions is different
void CircularMaze::stepBallWithCollisions(Ball& ball,
                                          float max_step_px,
                                          uint8_t max_substeps) {
    float dx, dy;
    if (!ball.consumeDelta(dx, dy)) return; // no motion this frame

    if (max_step_px <= 0.0f) max_step_px = ball.getRadius() * 0.5f;
    float max_axis = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
    int steps = (int)ceilf(max_axis / max_step_px);
    if (steps < 1) steps = 1;
    if ((uint8_t)steps > max_substeps) steps = max_substeps;

    float sx = dx / steps;
    float sy = dy / steps;

    for (int i = 0; i < steps; ++i) {
        ball.translate(sx, sy);
        handleCollisions(ball);   // resolve against arcs/spokes per sub-step
    }
}
