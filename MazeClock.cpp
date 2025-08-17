#include "MazeClock.h"
#include "I2C_BM8563.h"
#include <lvgl.h>
#include <math.h>

extern I2C_BM8563 rtc;


MazeClock::MazeClock(int rings, int spacing) : CircularMaze(rings, 12, spacing) {
    hour_arc = nullptr;
    minute_arc = nullptr;
}

// Main difference is the outermost spokes are drawn acting as both the radial axes of the maze adn the digit markers of the clock
void MazeClock::draw(lv_obj_t* parent, bool animate) {
    generate();
    wall_buffer_idx = 0; // Reset buffer index each time we redraw

    // LVGL One-time style init for wall lines
    static lv_style_t style_wall_circular;
    lv_style_init(&style_wall_circular);
    lv_style_set_line_width(&style_wall_circular, 2);
    lv_style_set_line_color(&style_wall_circular, lv_color_white());
    lv_style_set_line_rounded(&style_wall_circular, true);

    const float angle_step = 2 * M_PI / SECTORS_PER_RING;

    // Draw Radial Walls (Spokes) note that r < NUM_RINGS not r < NUM_RINGS - 1
    for (int r = 1; r < NUM_RINGS; r++) {
        for (int s = 0; s < SECTORS_PER_RING; s++) {
            if (r < NUM_RINGS && radial_walls[r][s]) {
                float angle = s * angle_step;
                float r1 = (r + 1) * RING_SPACING;
                float r2 = (r + 2) * RING_SPACING;

                if (wall_buffer_idx < point_buffer.size()) {
                    point_buffer[wall_buffer_idx][0] = {(lv_coord_t)(CENTER_X + cos(angle) * r1), (lv_coord_t)(CENTER_Y + sin(angle) * r1)};
                    point_buffer[wall_buffer_idx][1] = {(lv_coord_t)(CENTER_X + cos(angle) * r2), (lv_coord_t)(CENTER_Y + sin(angle) * r2)};
                    
                    lv_obj_t *wall = lv_line_create(parent);
                    lv_line_set_points(wall, point_buffer[wall_buffer_idx].data(), 2);
                    lv_obj_add_style(wall, &style_wall_circular, 0);
                    wall_buffer_idx++;
                    if (animate) lv_timer_handler();
                }
            }
        }
    }
    
    // Draw Circular Walls (Arcs)
    for (int r = 1; r < NUM_RINGS; r++) {
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

    // Clock LVGL
    I2C_BM8563_TimeTypeDef timeStruct;
    rtc.getTime(&timeStruct);

    static lv_style_t style_hour_arc, style_minute_arc;

    // Style for the red hour arc
    lv_style_init(&style_hour_arc);
    lv_style_set_arc_color(&style_hour_arc, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_arc_width(&style_hour_arc, 10); // Set arc thickness
    lv_style_set_arc_rounded(&style_hour_arc, true);

    // Style for the blue minute arc
    lv_style_init(&style_minute_arc);
    lv_style_set_arc_color(&style_minute_arc, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_arc_width(&style_minute_arc, 8);
    lv_style_set_arc_rounded(&style_minute_arc, true);

    // Draws the numbers 1 through 12, aligned with the maze spokes.
    static lv_style_t style_clock_num;
    lv_style_init(&style_clock_num);
    lv_style_set_text_color(&style_clock_num, lv_color_white());

    float radius = (NUM_RINGS + 2) * RING_SPACING;

    // Loop through each sector `s` from 0 to 11
    for (int s = 0; s < SECTORS_PER_RING; s++) {
        float angle = s * angle_step;

        lv_coord_t x = (lv_coord_t)(radius * cos(angle));
        lv_coord_t y = (lv_coord_t)(radius * sin(angle));

        lv_obj_t* label = lv_label_create(parent);
        lv_obj_add_style(label, &style_clock_num, 0);

        // This formula maps sector 0 (3 o'clock) to hour 3, sector 9 (12 o'clock) to hour 12, etc.
        int hour_to_display = (s + 2) % 12 + 1;
        lv_label_set_text_fmt(label, "%d", hour_to_display);

        lv_obj_center(label);
        lv_obj_set_pos(label, x, y);
    }

    // Hour indicator arc
    hour_arc = lv_arc_create(parent);
    lv_obj_add_style(hour_arc, &style_hour_arc, LV_PART_INDICATOR);
    lv_obj_remove_style(hour_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_style(hour_arc, NULL, LV_PART_MAIN);

    // Calculate the hour's position as an angle from 0 to 360
    float hour_value = (timeStruct.hours % 12) + (timeStruct.minutes / 60.0f);
    float hour_center_angle = (hour_value / 12.0f) * 360;

    // Define the arc segment's length (e.g., 20 degrees wide)
    int hour_arc_length = 20;
    // Set the start and end angles for the moving segment
    lv_arc_set_start_angle(hour_arc, hour_center_angle - (hour_arc_length / 2));
    lv_arc_set_end_angle(hour_arc, hour_center_angle + (hour_arc_length / 2));

    // Rotate the entire widget so that 0 degrees is at the top
    lv_arc_set_rotation(hour_arc, 270);

    // Set the size and position of the arc widget
    int hour_diameter = (NUM_RINGS+1) * RING_SPACING * 2;
    lv_obj_set_size(hour_arc, hour_diameter, hour_diameter);
    lv_obj_align(hour_arc, LV_ALIGN_CENTER, 0, 0);


    // Minute indicator arc 
    minute_arc = lv_arc_create(parent);
    lv_obj_add_style(minute_arc, &style_minute_arc, LV_PART_INDICATOR);
    lv_obj_remove_style(minute_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_style(minute_arc, NULL, LV_PART_MAIN);

    // Calculate the minute's position as an angle from 0 to 360
    float minute_center_angle = (timeStruct.minutes / 60.0f) * 360;

    int minute_arc_length = 12;
    lv_arc_set_start_angle(minute_arc, minute_center_angle - (minute_arc_length / 2));
    lv_arc_set_end_angle(minute_arc, minute_center_angle + (minute_arc_length / 2));

    // Rotate the entire widget so that 0 degrees is at the top
    lv_arc_set_rotation(minute_arc, 270);

    // Set a slightly smaller size for the minute arc to nest it
    int minute_diameter = (NUM_RINGS+1) * RING_SPACING * 2;
    lv_obj_set_size(minute_arc, minute_diameter, minute_diameter);
    lv_obj_align(minute_arc, LV_ALIGN_CENTER, 0, 0);

    // Final actual draw to screen
    lv_timer_handler();
}

void MazeClock::updateTime() {
    // Ensure the arc objects have been created before trying to update them
    if (!hour_arc || !minute_arc) {
        return;
    }

    // Get the current time from the RTC
    I2C_BM8563_TimeTypeDef timeStruct;
    rtc.getTime(&timeStruct);
    
    // Update hour arc
    float hour_value = (timeStruct.hours % 12) + (timeStruct.minutes / 60.0f);
    float hour_center_angle = (hour_value / 12.0f) * 360;
    int hour_arc_length = 20;
    lv_arc_set_start_angle(hour_arc, hour_center_angle - (hour_arc_length / 2));
    lv_arc_set_end_angle(hour_arc, hour_center_angle + (hour_arc_length / 2));

    // Update minute arc
    float minute_center_angle = (timeStruct.minutes / 60.0f) * 360;
    int minute_arc_length = 15;
    lv_arc_set_start_angle(minute_arc, minute_center_angle - (minute_arc_length / 2));
    lv_arc_set_end_angle(minute_arc, minute_center_angle + (minute_arc_length / 2));
}