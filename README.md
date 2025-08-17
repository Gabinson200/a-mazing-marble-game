# a-mazing-marble-game
Marble labyrinth game for the seeed studio XIAO Nrf Sense board and round display.

The aim of this project is to reproduce a small handheld marble maze game that many of us played in childhood, with the twist that we
can generate infinite amount of maze configurations for unlimited fun.

# Gameplay
Rotate the board and round screen around to control the ball and guide it along the maze until the exit is reached.
Once the exit is reached a new maze will be generated.
Please feel free to play around with the types and sizes of the mazes which will impact the difficulty of the maze. 
You can also toggle if you want to animate or instantly generate the maze with the `animate` parameter of the maze `draw` function. 

<p float="top">
  <img src="https://github.com/user-attachments/assets/ff8069d7-2971-4164-a0ce-230ea81355b2" width=32% height=50%>
  <img src="https://github.com/user-attachments/assets/0fd48ebc-94fd-4893-91c3-094c97c164ce" width=32% height=50%>
  <img src="https://github.com/user-attachments/assets/cd95b6cb-7dfd-4ab7-9b3d-a87f6c2d5348" width=32% height=50%>
</p>



## Hardware
- [Board](https://wiki.seeedstudio.com/XIAO_BLE/): Seeed Studio XIAO nRF52840 Sense
- [Display](https://wiki.seeedstudio.com/get_start_round_display/): Seeed Studio Round Display for XIAO
### Hardware sub components:
- On-board IMU: [LSM6DS3 IMU Sensor](https://wiki.seeedstudio.com/XIAO-BLE-Sense-IMU-Usage/)
- Real Time Clock (RTC) on the round display: [I2C BM8563](https://wiki.seeedstudio.com/seeedstudio_round_display_usage/#rtc-function)

## Arduino IDE setup
   - Follow the XIAO Nrf Sense board setup [here](https://wiki.seeedstudio.com/XIAO_BLE/#software-setup)
   - Follow the round display setup [here](https://wiki.seeedstudio.com/get_start_round_display/#software-preparation)
   - Note: We will be using [this](https://github.com/Seeed-Projects/SeeedStudio_lvgl) ported Lvgl 8.3 library
       
**Final Libraries List**  
   - [`lvgl 8.3`](https://github.com/Seeed-Projects/SeeedStudio_lvgl)
   - [`ArduinoGFX`](https://github.com/moononournation/Arduino_GFX/tree/master)
   - [`lv_xiao_round_screen`](https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay)
   - [`I2C_BM8563`](https://github.com/tanakamasayuki/I2C_BM8563)
   - [`LSM6DS3`](https://github.com/Seeed-Studio/Seeed_Arduino_LSM6DS3)

## Run
Set the MazeChoice variable to the desired maze type and uppload to board. 
(Note: for the clock maze you will have to initialize the RTC with the setup code beforehand)

## Class relationships
```
                          +------------------------------------+
                          |          Maze (abstract)           |
                          |------------------------------------|
                          | + generate()                       |
                          | + draw(parent, animate)            |
                          | + stepBallWithCollisions(Ball&,..) |
                          | + handleCollisions(Ball&)          |
                          | + getBallSpawnPixel()              |
                          | + getExitPixel()                   |
                          | + isAtExit(cx, cy, tol_px)         |
                          | + updateTime()                     |
                          +------------------^-----------------+
                                             |
            +--------------------------------+-----------------------------+
            |                                |                             |
+-------------------------+      +-------------------------+     +------------------------+
|     RectangularMaze     |      |      CircularMaze       |     |       MazeClock        |
|-------------------------|      |-------------------------|     |------------------------|
| horiz_walls[ROWS+1][C], |      | circular_walls[R][S],   |     | clock-specific draw/   |
| vert_walls[R][COLS+1]   |      | radial_walls[R-1][S]    |     | updateTime() logic     |
+-------------------------+      +-------------------------+     +------------------------+

                   (composition / usage – created/owned by the sketch)
                                            |
                                            v
                               +-----------------------------+
                               |          Ball               |
                               |-----------------------------|
                               | center: (x, y), radius      |
                               | velocity: (vx, vy)          |
                               | +updatePhysics(roll,pitch)  |
                               | +translate(dx, dy)          |
                               | +consumeDelta(dx, dy)       |
                               | +draw()  (LVGL object)      |
                               +-----------------------------+
                                            ^
                                            |
                               +-----------------------------+
                               |            IMU              |
                               |-----------------------------|
                               | +begin(), +read()           |
                               | +getRollAndPitch(roll,pitch)|
                               +-----------------------------+
          
                                     (both draw into)
                                            v
                                  +--------------------+
                                  |        LVGL        |
                                  |  screen + widgets  |
                                  +--------------------+
```

## Future Additions
- Optimize code for reduced latency
- Add randomly placed "holes" or "fire walls" that when touched reset the player to the start, adding difficulty.
- Add startscreen / menu so maze types and difficulty can be changed on device (Some kind of IMU controlled UI selection could be interesting, if not use the touchscreen)
  
This project was a way for me to learn / relearn some C++ and more specifically object oriented style of programming after years of data sci Python so sorry if its a little noobish. 
If you have read this far please consider starring the repo. :kissing_closed_eyes:
