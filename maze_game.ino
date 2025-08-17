#include <Arduino.h>
#include <lvgl.h>
#include "lv_xiao_round_screen.h"
#include "IMU.h"
#include "RectangularMaze.h"
#include "CircularMaze.h"
#include "I2C_BM8563.h"
#include "MazeClock.h"
#include "Ball.h"

// Screen dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

// LVGL Draw Buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];

// RTC
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);
uint32_t last_time_update = 0;

// Global objects
Maze* maze = nullptr; // Base class pointer
IMU imu;
Ball* ball = nullptr; 

enum class MazeType : uint8_t { Rectangular, Circular, Clock };

// >>> Set your choice here <<<
constexpr MazeType MazeChoice = MazeType::Rectangular;  // Rectangular | Circular | Clock

static Maze* createMaze(MazeType t) {
    switch (t) {
        case MazeType::Rectangular:
            // rows, cols, cell_size, offset
            // Given we usually want a centered square maze of size nxn we can calculate cell size as 
            // cell size = [screen size (240) - 2*offset] / n or just 160 / n
            return new RectangularMaze(10, 10, 16, 40);
        case MazeType::Circular:
            // rings, sectors, spacing
            // if we want n rings we can caulculate rign spacing as
            // [screen size (240) / 2] / n ==>  120/n - 1 (for some extra space for the last ring)
            // all the way down until ring spacing == 7
            return new CircularMaze(10, 16, 11);
        case MazeType::Clock:
        default:
            // hours, ring spacing
            return new MazeClock(6, 12);
    }
}

// optimize this later
static void regenerateCurrentMaze() {
    // Delete ball first
    if (ball) { delete ball; ball = nullptr; }

    // Clear old maze visuals from the screen
    lv_obj_t* screen = lv_scr_act();
    lv_obj_clean(screen);

    // Recreate the same type of maze
    if (maze) { delete maze; maze = nullptr; }
    maze = createMaze(MazeChoice);
    maze->generate();
    maze->draw(screen, /*animate=*/true);

    // Spawn a new ball at the new maze’s spawn
    lv_point_t spawn = maze->getBallSpawnPixel();
    ball = new Ball(screen, spawn.x, spawn.y, /*radius=*/5.0f);
}

void setup() {
    Serial.begin(115200);

    Wire.begin();
    rtc.begin();

    lv_init();
    lv_xiao_disp_init();
    // Use pin noise for random number generation
    randomSeed(analogRead(A0));

    // Create a main screen
    lv_obj_t* mainScreen = lv_obj_create(nullptr);
    lv_obj_set_size(mainScreen, lv_disp_get_hor_res(nullptr), lv_disp_get_ver_res(nullptr));
    lv_scr_load(mainScreen);
    lv_obj_set_style_bg_color(mainScreen, lv_color_black(), 0);

    // Initialize the IMU
    imu.begin();

    // Choose which maze to create
    maze = createMaze(MazeChoice);

    // Generate and draw the maze
    if (maze) {
        maze->generate();
        // Animate the drawing, or set to false if you want instant maze generation
        maze->draw(mainScreen, false);

        lv_point_t spawn = maze->getBallSpawnPixel();
        Serial.print("spawn location: ");
        Serial.println(spawn.x);
        Serial.println(spawn.y);
        // choose your ball radius; if you keep default 5.0, pass that here to set the member correctly
        ball = new Ball(mainScreen, spawn.x, spawn.y, 5.0f);

    }
}

void loop() {
    float roll = 0.0f, pitch = 0.0f;

    // Check if 60 seconds have passed since the last update
    if (millis() - last_time_update > 60000) {
        last_time_update = millis();
        if (maze) {
            maze->updateTime(); // Call the new update function
            Serial.println("Time updated");
        }
    }

    // Read IMU data if motion is detected
    if (imu.read()) {
        imu.getRollAndPitch(roll, pitch);
    }

    // Update ball position based on IMU data
    if (ball) {
        ball->updatePhysics(roll, pitch);
        maze->stepBallWithCollisions(*ball,
            /*max_step_px=*/ ball->getRadius() * 0.5f,  // tune: smaller => safer
            /*max_substeps=*/ 24                         // cap for performance
        );
        ball->draw();
    }

    // check if exit is reached
    const float tol = ball->getRadius() + 4.0f;

    if (maze->isAtExit(ball->getX(), ball->getY(), tol)) {
        regenerateCurrentMaze();
        // optional: brief pause to avoid instant retrigger
        delay(40);
        return; // skip the rest of this loop iteration
    }

    lv_timer_handler();
    delay(5);
}