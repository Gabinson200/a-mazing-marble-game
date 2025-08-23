#include <Arduino.h>
#include <lvgl.h>
#include "lv_xiao_round_screen.h"
#include "IMU.h"
#include "RectangularMaze.h"
#include "CircularMaze.h"
#include "I2C_BM8563.h"
#include "MazeClock.h"
#include "GestureAI.h"
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

// AI gesture recognition
GestureAI gesture;
const char* GESTURE_LABELS[GESTURE_NUM_CLASSES] = {
    "bow","sleep","circle" // <-- change to your model's order
};
lv_obj_t* gGestureLabel = nullptr;
static const float GESTURE_FIRE_THRESH = 0.50;

enum class MazeType : uint8_t { Rectangular, Circular, Clock };

static bool animateMaze = false;

// Difficulty state
static uint8_t rect_level  = 0;  // selects n for NxN
static uint8_t circ_level  = 0;  // selects {rings,sectors}
static uint8_t clock_level = 0;  // selects rings(hours)

// Simple ladders you can tune anytime:
static const uint8_t RECT_N_OPTS[]      = { 8, 10,  12, 16 };
static const uint8_t CIRC_RINGS_OPTS[]  = { 6,  8,  10, 12 };
static const uint8_t CIRC_SECT_OPTS[]   = { 10, 12, 16, 18 };   // spokes
static const uint8_t CLOCK_RINGS_OPTS[] = { 7, 8, 9};

// >>> Set your choice here <<<
static MazeType MazeChoice = MazeType::Clock;  // Rectangular | Circular | Clock


static Maze* createMaze(MazeType t) {
    switch (t) {
        case MazeType::Rectangular: {
            // Use NxN and compute: cell offset ≈ (240 - 2*offset)/N
            const int offset   = 40;
            const int N        = RECT_N_OPTS[rect_level % (sizeof(RECT_N_OPTS)/sizeof(RECT_N_OPTS[0]))];
            const int cell     = max(2, (240 - 2*offset) / N);
            const int cols     = N;
            const int rows     = N;
            return new RectangularMaze(cols, rows, cell, offset); // (cols, rows, cell, offset)
        }

        case MazeType::Circular: {
            // rings/sectors;  spacing ≈ 120/rings - 1, clamp to ≥7
            const int rings    = CIRC_RINGS_OPTS[circ_level % (sizeof(CIRC_RINGS_OPTS)/sizeof(CIRC_RINGS_OPTS[0]))];
            const int sectors  = CIRC_SECT_OPTS [circ_level % (sizeof(CIRC_SECT_OPTS)/sizeof(CIRC_SECT_OPTS[0]))];
            const int spacing  = max(7, (120 / max(1, rings)) - 1);
            return new CircularMaze(rings, sectors, spacing);     // (rings, sectors, spacing)
        }

        case MazeType::Clock:
        default: {
            // MazeClock(rings, spacing); 12 sectors embedded in the clock maze
            const int rings    = CLOCK_RINGS_OPTS[clock_level % (sizeof(CLOCK_RINGS_OPTS)/sizeof(CLOCK_RINGS_OPTS[0]))];
            const int spacing  = max(4, ((120 / rings)-1) - (2*(120 / rings) / rings));
            Serial.println(rings);
            Serial.println(spacing);
            return new MazeClock(rings, spacing);                 // (rings, spacing)
        }
    }
}

static void cycleMazeType() {
    // cycle Rectangular → Circular → Clock → Rectangular
    if      (MazeChoice == MazeType::Rectangular) MazeChoice = MazeType::Circular;
    else if (MazeChoice == MazeType::Circular)    MazeChoice = MazeType::Clock;
    else                                          MazeChoice = MazeType::Rectangular;

    regenerateCurrentMaze();  // deletes old ball/maze and recreates via createMaze(MazeChoice)
}

static void cycleCurrentDifficulty() {
    switch (MazeChoice) {
        case MazeType::Rectangular:
            rect_level = (rect_level + 1) % (sizeof(RECT_N_OPTS)/sizeof(RECT_N_OPTS[0]));
            break;
        case MazeType::Circular:
            circ_level = (circ_level + 1) % min(
                sizeof(CIRC_RINGS_OPTS)/sizeof(CIRC_RINGS_OPTS[0]),
                sizeof(CIRC_SECT_OPTS)/sizeof(CIRC_SECT_OPTS[0]));
            break;
        case MazeType::Clock:
        default:
            clock_level = (clock_level + 1) % (sizeof(CLOCK_RINGS_OPTS)/sizeof(CLOCK_RINGS_OPTS[0]));
            break;
    }
    regenerateCurrentMaze();  // cleanly delete + rebuild
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
    maze->draw(screen, /*animate=*/animateMaze);

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

    // after your display/LVGL init:
    gGestureLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(gGestureLabel, &lv_font_montserrat_14, 0); // or your font
    lv_label_set_text(gGestureLabel, "");                                 // start empty
    lv_obj_align(gGestureLabel, LV_ALIGN_TOP_MID, 0, 4);

    // Initialize the IMU
    imu.begin();
    if (!imu.begin()) {
        Serial.println("IMU failed to init");
    }

    if (!gesture.begin()) {
        Serial.println("Gesture/TFLM failed to init");
    }


    // Choose which maze to create
    maze = createMaze(MazeChoice);

    // Generate and draw the maze
    if (maze) {
        maze->generate();
        // Animate the drawing, or set to false if you want instant maze generation
        maze->draw(mainScreen, animateMaze);

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
    if(MazeChoice == MazeType::Clock){
        if (millis() - last_time_update > 60000) {
            last_time_update = millis();
            maze->updateTime(); // Call the new update function
            Serial.println("Time updated");
        }
    }

    // Read IMU data if motion is detected
    if (imu.read()) {
        imu.getRollAndPitch(roll, pitch);
    }

    float probs[GESTURE_NUM_CLASSES];
    int top = -1;

    // This returns immediately if threshold NOT reached.
    if (gesture.runIfTriggered(imu, probs, &top)) {
        // Print results
        Serial.println("---- Gesture results ----");
        for (int i = 0; i < GESTURE_NUM_CLASSES; ++i) {
            Serial.print(GESTURE_LABELS[i]); Serial.print(": ");
            Serial.println(probs[i], 4);
        }
        if (top >= 0 && top < GESTURE_NUM_CLASSES) {
            char buf[96];
            snprintf(buf, sizeof(buf), "Gesture: %s (%.2f)", GESTURE_LABELS[top], probs[top]);
            //if (gGestureLabel) lv_label_set_text(gGestureLabel, buf);
            Serial.println(buf);
        }

        if(probs[top] >= GESTURE_FIRE_THRESH){
            switch(top){
                case 0: // bow
                    Serial.println("Cycle DIFFICULTY");
                    cycleCurrentDifficulty();
                    break;

                case 1: // sleep
                    Serial.println("tToggle animate");
                    animateMaze = !animateMaze;
                    Serial.println(animateMaze ? "ON" : "OFF");
                    break;

                case 2: // circle
                    Serial.println("Cycle maze TYPE");
                    cycleMazeType();
                    break;

            }
        }else{
            Serial.println("[Gesture] top < 0.50 → ignore");
        }
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