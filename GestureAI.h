#pragma once
#include <Arduino.h>
#include "IMU.h"
#include "model.h" 

// TFLM headers
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

// --- Configure to your model ---
#ifndef GESTURE_NUM_CLASSES
#define GESTURE_NUM_CLASSES 3  // change to match your model output size
#endif

#ifndef GESTURE_SAMPLES
#define GESTURE_SAMPLES 180    // ~1.5s window at 200Hz
#endif


#ifndef GESTURE_ACCEL_START_THRESH
#define GESTURE_ACCEL_START_THRESH 2.3f  // sum|ax|+|ay|+|az| to trigger capture (tweak)
#endif

class GestureAI {
public:
  GestureAI() : interpreter_(nullptr), input_(nullptr), output_(nullptr),
                input_scale_(1.f), input_zero_point_(0),
                output_scale_(1.f), output_zero_point_(0) {}

  bool begin();

  bool runIfTriggered(IMU& imu, float* probs_out, int* top_idx_out);

private:
  // TFLM
  tflite::MicroInterpreter* interpreter_;
  TfLiteTensor* input_;
  TfLiteTensor* output_;
  float input_scale_, output_scale_;
  int   input_zero_point_, output_zero_point_;

  // Memory arena (tune as needed depending on model)
  static constexpr int kArenaSize = 6 * 1024; // start here; increase if AllocateTensors fails
  alignas(16) static uint8_t arena_[kArenaSize];

  // Helpers
  inline void quantizeAndStore_(int sample_idx, float ax, float ay, float az,
                                float gx, float gy, float gz);

  bool doCaptureAndInfer_(IMU& imu, float* probs_out, int* top_idx_out);
};
