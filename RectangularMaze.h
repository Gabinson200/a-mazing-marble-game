//RectangularMaze.h

#ifndef RECTANGULAR_MAZE_H
#define RECTANGULAR_MAZE_H

#include "Maze.h"
#include <vector>
#include <array>
#include "Ball.h"

class RectangularMaze : public Maze {
public:
    /**
     * @brief Constructor for a dynamically-sized rectangular maze.
     * @param cols The number of columns in the maze.
     * @param rows The number of rows in the maze.
     * @param cell_size The size of each cell in pixels.
     * @param offset The pixel offset from the top left screen edge.
     */
    RectangularMaze(int cols, int rows, int cell_size, int offset);

    /**
     * @brief Resets the horizontal, vertical and visited cells 2D vectors and starts the maze carving at a random point.
     */
    virtual void generate() override;

    /**
     * @brief Draws the maze and intial ball / exit positions
     * @param parent LVGL screen to draw to
     * @param animate boolean, set true if you want to see maze drawing animation
     */
    virtual void draw(lv_obj_t* parent, bool animate) override;

    /**
     * @brief Gets ball position, simple collision check with nearby walls, moves ball outside of collision area, "bouncess" off wall 
     * @param ball Ball object
     */
    virtual void handleCollisions(Ball& ball) override;

    /**
     * @brief Subsamples ball movement and calls collision check on suub-distances so we dont accidentally "tunnel"
     * @param ball Ball obj
     * @param max_step_px Maximum pixels length a substep can be
     * @param max_substeps Maximum number of substeps a movement reading can be divided into
     */
    virtual void stepBallWithCollisions(Ball& ball,
                                    float max_step_px = -1.0f,
                                    uint8_t max_substeps = 32) override;

    // Getters for the ball and exit spawn locations
    lv_point_t getBallSpawnPixel() const override { return ball_spawn_px; }

    // note we return the center pixel of the exit box 
    lv_point_t getExitPixel() const override { return {exit_px.x + CELL_SIZE/2, exit_px.y + CELL_SIZE/2}; }

private:
    int COLS; // = 8;
    int ROWS; //  = 8;
    int CELL_SIZE; // = 20;
    int OFFSET; // = 40;

    std::vector<std::vector<bool>> horiz_walls;
    std::vector<std::vector<bool>> vert_walls;
    std::vector<std::vector<bool>> visited_cells;
    
    std::vector<std::array<lv_point_t, 2>> wall_points;
    int wall_count;

    // Ball and exit spawn coord variables
    int exit_r = 0, exit_c = 0;
    int spawn_r = 0, spawn_c = 0;
    lv_point_t ball_spawn_px = {0,0};
    lv_point_t exit_px = {0,0};

    /**
     * @brief Recursive DFS for maze carving.
     * @param r row index
     * @param c column index
     */
    void carve(int r, int c);

    /**
     * @brief Picks exit on perimeter, sets opposite  coordinate as spawn
     */
    void placeExitAndSpawn(); 

    /**
     * @brief Returns a random lv_poimt_t that is on the perimeter of the maze NOT CURRENTLY USED
     */
    lv_point_t randomPerimeterCoord();

};

#endif // RECTANGULAR_MAZE_H