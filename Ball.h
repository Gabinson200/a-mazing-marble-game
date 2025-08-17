// Ball.h

#ifndef BALL_H
#define BALL_H

#include <lvgl.h>

class Ball {
public:
    /**
     * @brief Constructs a new Ball object.
     * @param parent The parent LVGL object to draw the ball on.
     * @param startX The initial X coordinate of the ball's center.
     * @param startY The initial Y coordinate of the ball's center.
     * @param radius The radius of the ball.
     */
    Ball(lv_obj_t* parent, float start_x, float start_y, float radius);

    /**
     * @brief Destructor of Ball object.
    */
    ~Ball() {
        if (obj) {
            lv_obj_del(obj);
            obj = nullptr;
        }
    }
    
    /**
     * @brief Applies forces from the IMU to update the ball's velocity.
     * @param roll The roll angle (tilt) in degrees.
     * @param pitch The pitch angle (tilt) in degrees.
     */
    void updatePhysics(float roll, float pitch);
    
    /**
     * @brief Computes change in x and y dir of ball and returns true if 
     * non zero, used for collision checking
     * @param dx Change in the x direction.
     * @param dy Change in the y direction.
     */
    bool consumeDelta(float& dx, float& dy);

    /**
     * @brief Actually moves ball by dx, dy
     * @param dx Change in the x direction.
     * @param dy Change in the y direction.
     */
    void translate(float dx, float dy);

    // Updates the on-screen LVGL object's position to match the internal coordinates
    void draw();
    
    // Getters and Setters for the Maze to use
    float getX() const { return x; }
    float getY() const { return y; }
    void setX(float nx) { x = nx; }
    void setY(float ny) { y = ny; }

    float getVelocityX() const { return velocity_x; }
    float getVelocityY() const { return velocity_y; }
    void setVelocityX(float vx) { velocity_x = vx; }
    void setVelocityY(float vy) { velocity_y = vy; }
    float getRadius() const { return radius; }
    uint32_t getLastUpdateMs() const { return last_update_ms; }

private:
    lv_obj_t* obj; // Pointer to the LVGL object for the ball

    // State variables
    float x;
    float y;
    float velocity_x;
    float velocity_y;
    uint32_t last_update_ms;
    
    // Properties
    float radius = 5.0f;
};

#endif // BALL_H