// maze.h

#ifndef MAZE_H
#define MAZE_H

#include <lvgl.h>

class Ball; // have to forward declare ball class here

class Maze {
public:
    /**
     * @brief Destructor.
     */
    virtual ~Maze() {}

    // Pure virtual functions that maze subclasses must implement
    // generate / draw will generate and draw the mazes as the names suggest
    virtual void generate() = 0;

    virtual void draw(lv_obj_t* parent, bool animate) = 0;

    // These are just two getters so the maze knows where to initially draw the ball and exit
    virtual lv_point_t getBallSpawnPixel() const { return {120,120}; }
    virtual lv_point_t getExitPixel() const { return {0,0}; }

    /**
     * @brief Simple check if ball is at exit
     * @param cx ball x coord
     * @param cy ball y coord
     * @param tol_px num pixel range ball has to be to exit to count
     */
    virtual bool isAtExit(float cx, float cy, float tol_px = 10.0f) const {
        lv_point_t e = getExitPixel();
        const float dx = cx - static_cast<float>(e.x);
        const float dy = cy - static_cast<float>(e.y);
        return (dx*dx + dy*dy) <= (tol_px * tol_px);
    }

    // These handle collision methods will do collision checking w subsampling for the two maze types
    virtual void handleCollisions(Ball& ball) = 0;

    virtual void stepBallWithCollisions(Ball& ball,
                                    float max_step_px = -1.0f,
                                    uint8_t max_substeps = 32) = 0;

    // updates RTC time for maze clock, might move to maze clock class as we will probaby never have 
    // a rectangular clock maze
    virtual void updateTime() {}; // just keep it no-op for now
};

#endif // MAZE_H