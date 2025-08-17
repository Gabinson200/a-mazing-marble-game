#include "Ball.h"
#include <Arduino.h> // For millis()


Ball::Ball(lv_obj_t* parent, float start_x, float start_y, float radius) {
    x = start_x;
    y = start_y;
    velocity_x = 0.0f;
    velocity_y = 0.0f;
    this->radius = radius;
    last_update_ms = millis();

    // Create the LVGL object for the ball
    obj = lv_obj_create(parent);
    lv_obj_set_size(obj, radius * 2, radius * 2);

    static lv_style_t style_ball;
    lv_style_init(&style_ball);
    lv_style_set_bg_color(&style_ball, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_radius(&style_ball, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_ball, 0);
    lv_obj_add_style(obj, &style_ball, 0);
    
    draw(); // Draw initial position
}


void Ball::updatePhysics(float roll, float pitch) {
    // Acceleration is proportional to the tilt
    const float gravity_factor = 0.05f;
    velocity_x += -pitch * gravity_factor;
    velocity_y += -roll * gravity_factor;

    // Apply some friction to slow the ball down over time
    const float friction = 0.98f;
    velocity_x *= friction;
    velocity_y *= friction;
}


void Ball::draw() {
    // Update the on-screen object's position
    lv_obj_set_pos(obj, (lv_coord_t)(x - radius), (lv_coord_t)(y - radius));
}


// Updates dx and xy returns true if they change, note that IMU readings are only sent if a large enough change is detected
bool Ball::consumeDelta(float& dx, float& dy) {
    uint32_t now = millis();
    float time_change = (now - last_update_ms) / 1000.0f;
    last_update_ms = now;

    const float scale_factor = 10.0f;
    dx = velocity_x * time_change * scale_factor;
    dy = velocity_y * time_change * scale_factor;
    return (dx != 0.0f || dy != 0.0f);
}

// Actually applies movement to position
void Ball::translate(float dx, float dy) {
    x += dx;
    y += dy;
}
