#include "IMU.h"
#include <Arduino.h>

IMU::IMU(float threshold, int samples)
    : myIMU(I2C_MODE, 0x6A),
      turnThreshold(threshold),
      numSamples(samples),
      samplesRead(samples),
      rollAvg(0.0f),
      pitchAvg(0.0f) {}

bool IMU::begin() {
    Wire.begin();
    if (myIMU.begin() != 0) {
        Serial.println("IMU device error");
        return false;
    }
    Serial.println("IMU initialized successfully.");
    return true;
}

void IMU::getRollAndPitch(float& roll, float& pitch) const {
    roll = rollAvg;
    pitch = pitchAvg;
}

bool IMU::read() {
    float gX = myIMU.readFloatGyroX();
    float gY = myIMU.readFloatGyroY();
    float gZ = myIMU.readFloatGyroZ();

    if (fabs(gX) + fabs(gY) + fabs(gZ) < turnThreshold) {
        return false; // No significant motion
    }

    // Motion detected, take samples
    float rollSum = 0;
    float pitchSum = 0;

    for (int i = 0; i < numSamples; ++i) {
        float aX = myIMU.readFloatAccelX();
        float aY = myIMU.readFloatAccelY();
        float aZ = myIMU.readFloatAccelZ();

        float pitchAcc = atan2(-aX, sqrt(aY * aY + aZ * aZ)) * 180.0 / PI;
        float rawRoll = atan2(aY, aZ) * 180.0 / PI;

        // Clamp roll to –90 to +90
        float rollAcc = (rawRoll > 90)  ? rawRoll - 180 :
                        (rawRoll < -90) ? rawRoll + 180 :
                                          rawRoll;
        
        rollSum += rollAcc;
        pitchSum += pitchAcc;
    }

    rollAvg = rollSum / numSamples;
    pitchAvg = pitchSum / numSamples;
    
    //Serial.print("Roll: "); Serial.print(rollAvg, 1);
    //Serial.print(", Pitch: "); Serial.println(pitchAvg, 1);

    return true;
}

// IMU.cpp  (append near the end)
bool IMU::readAccel(float& ax, float& ay, float& az) {
    ax = myIMU.readFloatAccelX();
    ay = myIMU.readFloatAccelY();
    az = myIMU.readFloatAccelZ();
    return true;
}

bool IMU::readAll(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
    ax = myIMU.readFloatAccelX();
    ay = myIMU.readFloatAccelY();
    az = myIMU.readFloatAccelZ();
    gx = myIMU.readFloatGyroX();
    gy = myIMU.readFloatGyroY();
    gz = myIMU.readFloatGyroZ();
    return true;
}
