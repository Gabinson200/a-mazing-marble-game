#include "RectangularMaze.h"
#include <Arduino.h>

RectangularMaze::RectangularMaze(int cols, int rows, int cell_size, int offset) {
    COLS = cols;
    ROWS = rows;
    CELL_SIZE = cell_size;
    OFFSET = offset;

    // Resize all wall and cell tracking vectors
    horiz_walls.resize(ROWS + 1, std::vector<bool>(COLS));
    vert_walls.resize(ROWS, std::vector<bool>(COLS + 1));
    visited_cells.resize(ROWS, std::vector<bool>(COLS));
    int MAX_WALLS = (ROWS + 1) * COLS + ROWS * (COLS + 1);

    // Resize the LVGL point buffer
    wall_points.resize(MAX_WALLS);
}



void RectangularMaze::generate() {
    // Note the exclusive / inclusive less than symbols, this allows us to generate the outermost edges of the maze
    for (int r = 0; r <= ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) horiz_walls[r][c] = true;
    }
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c <= COLS; ++c) vert_walls[r][c] = true;
    }

    // Clear the visited cells vector
    for(auto& row : visited_cells) {
        std::fill(row.begin(), row.end(), false);
    }

    //get location of ball and exit
    placeExitAndSpawn();

    carve(random(COLS), random(ROWS));
}



void RectangularMaze::draw(lv_obj_t* parent, bool animate) {
    // This style is static, so it's initialized only once across all instances.
    static lv_style_t style_wall;
    static bool style_initialized = false;
    if (!style_initialized) {
        lv_style_init(&style_wall);
        lv_style_set_line_width(&style_wall, 2);
        lv_style_set_line_color(&style_wall, lv_color_white());
        lv_style_set_line_rounded(&style_wall, true);
        style_initialized = true;
    }
    // remove any existing walls/exit
    lv_obj_clean(parent);
    wall_count = 0; // Reset for redraw

    // Draw Horizontal walls
    for (int r = 0; r <= ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            if (horiz_walls[r][c]) {
                if (wall_count < wall_points.size()) {
                    wall_points[wall_count][0] = { (lv_coord_t)(c * CELL_SIZE + OFFSET), (lv_coord_t)(r * CELL_SIZE + OFFSET) };
                    wall_points[wall_count][1] = { (lv_coord_t)((c + 1) * CELL_SIZE + OFFSET), (lv_coord_t)(r * CELL_SIZE + OFFSET) };

                    lv_obj_t* wall = lv_line_create(parent);
                    lv_line_set_points(wall, wall_points[wall_count].data(), 2);
                    lv_obj_add_style(wall, &style_wall, 0);
                    wall_count++;
                    if(animate) lv_timer_handler();
                }
            }
        }
    }

    //  Draw Vertical walls
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c <= COLS; ++c) {
            if (vert_walls[r][c]) {
                if (wall_count < wall_points.size()) {
                    wall_points[wall_count][0] = { (lv_coord_t)(c * CELL_SIZE + OFFSET), (lv_coord_t)(r * CELL_SIZE + OFFSET) };
                    wall_points[wall_count][1] = { (lv_coord_t)(c * CELL_SIZE + OFFSET), (lv_coord_t)((r + 1) * CELL_SIZE + OFFSET) };

                    lv_obj_t* wall = lv_line_create(parent);
                    lv_line_set_points(wall, wall_points[wall_count].data(), 2);
                    lv_obj_add_style(wall, &style_wall, 0);
                    wall_count++;
                    if(animate) lv_timer_handler();
                }
            }
        }
    }


    // Draw Exit Cell (red)
    lv_obj_t* exitObj = lv_obj_create(parent);
    lv_obj_set_size(exitObj, CELL_SIZE-2, CELL_SIZE-2);
    lv_obj_set_pos(exitObj, exit_px.x, exit_px.y);
    lv_obj_set_style_bg_color(exitObj, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_border_width(exitObj, 0, 0);
    lv_obj_set_style_radius(exitObj, 0, 0); // makes it square

    // Final actual draw to screen
    lv_timer_handler();
}



void RectangularMaze::carve(int r, int c) {
  visited_cells[r][c] = true;
  const int dr[4] = {-1,1,0,0}, dc[4] = {0,0,-1,1};
  int dirs[4] = {0,1,2,3};
  // Fisher-Yates shuffle
  for (int i = 3; i > 0; i--) {
    int j = random(i+1);
    int t = dirs[i];
    dirs[i] = dirs[j];
    dirs[j] = t;
  }
  // might switch to stack implementation rather than recursive
  for (int i = 0; i < 4; i++) {
    int d = dirs[i];
    int nr = r + dr[d];
    int nc = c + dc[d];
    // checks in random order around cell if neighboring cells are visited, knocks down wall, recurses
    if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS && !visited_cells[nr][nc]) {
      if (d < 2) horiz_walls[max(r,nr)][c] = false;
      else if (d == 2) vert_walls[r][c] = false;
      else vert_walls[r][c+1] = false;
      carve(nr, nc);
    }
  }
}


void RectangularMaze::placeExitAndSpawn() {
    // Pick a random side & cell on that side
    int side = random(4); // 0=TOP,1=RIGHT,2=BOTTOM,3=LEFT
    switch (side) {
        case 0: exit_r = 0;            exit_c = random(COLS); break;
        case 1: exit_r = random(ROWS); exit_c = COLS-1; break;
        case 2: exit_r = ROWS-1;       exit_c = random(COLS); break;
        case 3: exit_r = random(ROWS); exit_c = 0; break;
    }

    // Opposite corner for spawn (mirror across center)
    spawn_r = (ROWS-1) - exit_r;
    spawn_c = (COLS-1) - exit_c;

    // Pixel coords
    // Note: lvgl zero index (0,0) is top left corner 
    exit_px = { (lv_coord_t)(exit_c*CELL_SIZE + OFFSET),
                (lv_coord_t)(exit_r*CELL_SIZE + OFFSET)};

    ball_spawn_px = { (lv_coord_t)(spawn_c*CELL_SIZE + OFFSET + CELL_SIZE/2),
                      (lv_coord_t)(spawn_r*CELL_SIZE + OFFSET + CELL_SIZE/2) };
}



void RectangularMaze::handleCollisions(Ball& ball) {
    float ball_x = ball.getX();
    float ball_y = ball.getY();
    float ball_r = ball.getRadius();

    int col = (int)((ball_x - OFFSET) / CELL_SIZE);
    int row = (int)((ball_y - OFFSET) / CELL_SIZE);

    // Clamp to the playable grid
    if (col < 0) col = 0;
    if (col > COLS - 1) col = COLS - 1;
    if (row < 0) row = 0;
    if (row > ROWS - 1) row = ROWS - 1;

    // Left wall
    if (vert_walls[row][col] && (ball_x - ball_r < col * CELL_SIZE + OFFSET)) {
        ball.setX(col * CELL_SIZE + OFFSET + ball_r);
        ball.setVelocityX(-ball.getVelocityX() * 0.25f);
    }
    // Right wall
    if (vert_walls[row][col + 1] && (ball_x + ball_r > (col + 1) * CELL_SIZE + OFFSET)) {
        ball.setX((col + 1) * CELL_SIZE + OFFSET - ball_r);
        ball.setVelocityX(-ball.getVelocityX() * 0.25f);
    }
    // Top wall
    if (horiz_walls[row][col] && (ball_y - ball_r < row * CELL_SIZE + OFFSET)) {
        ball.setY(row * CELL_SIZE + OFFSET + ball_r);
        ball.setVelocityY(-ball.getVelocityY() * 0.25f);
    }
    // Bottom wall
    if (horiz_walls[row + 1][col] && (ball_y + ball_r > (row + 1) * CELL_SIZE + OFFSET)) {
        ball.setY((row + 1) * CELL_SIZE + OFFSET - ball_r);
        ball.setVelocityY(-ball.getVelocityY() * 0.25f);
    }
}



void RectangularMaze::stepBallWithCollisions(Ball& ball,
                                             float max_step_px,
                                             uint8_t max_substeps) {
    float dx, dy;
    if (!ball.consumeDelta(dx, dy)) return; // no motion this frame

    // Choose a safe step length: default to half the radius
    if (max_step_px <= 0.0f) max_step_px = ball.getRadius() * 0.5f;

    // Determine steps based on direction of greatest change
    float max_axis = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
    int steps = (int)ceilf(max_axis / max_step_px);
    if (steps < 1) steps = 1;
    if ((uint8_t)steps > max_substeps) steps = max_substeps;

    float sx = dx / steps;
    float sy = dy / steps;

    for (int i = 0; i < steps; ++i) {
        ball.translate(sx, sy);
        handleCollisions(ball);  // clamp/reflect if we touched any wall
    }
}


// Might be a better random periemter generatro since only one random call is made
lv_point_t RectangularMaze::randomPerimeterCoord() {
    int P = 2*COLS + 2*ROWS - 4;
    int idx = random(0, P);
    int r, c;
    if      (idx < COLS)        { r = 0;       c = idx;           }
    else if ((idx -= COLS) < ROWS-2) { r = 1+idx;  c = COLS-1;      }
    else if ((idx -= ROWS-2) < COLS) { r = ROWS-1; c = COLS-1-idx; }
    else                           { idx -= COLS; r = ROWS-2-idx; c = 0; }
    // return in pixels, with OFFSET
    return { (lv_coord_t)(c*CELL_SIZE + OFFSET),
             (lv_coord_t)(r*CELL_SIZE + OFFSET) };
}
