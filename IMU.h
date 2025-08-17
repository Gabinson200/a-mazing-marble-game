// IMU.h

#ifndef IMU_H
#define IMU_H

#include <LSM6DS3.h>
#include <Wire.h>

class IMU {
public:
    /**
     * @brief Constructor, sets movement thershold to start reading and number of samples per reading to average.
     * @param threshold Gyro threshold to be reached for reading to begin
     * @param samples Number of samples per reading
     */
    IMU(float threshold = 5.0f, int samples = 10);

    /**
     * @brief Initializes Wire for IMU, returns true if IMU started successfully.
     */
    bool begin();

    /**
     * @brief If gyro readings rises above threshold, reads and averages samples amount of accelerometer data and return true.
     */
    bool read();

    /**
     * @brief Sets roll and pitch to last average roll and pitch IMU readings
     */
    void getRollAndPitch(float& roll, float& pitch) const;

private:
    LSM6DS3 myIMU;
    const float turnThreshold;
    const int numSamples;
    int samplesRead;
    float rollAvg;
    float pitchAvg;
};

#endif // IMU_H