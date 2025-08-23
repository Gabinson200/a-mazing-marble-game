#include "GestureAI.h"

uint8_t GestureAI::arena_[GestureAI::kArenaSize];

bool GestureAI::begin() {
  static tflite::MicroErrorReporter micro_error_reporter;
  static tflite::MicroMutableOpResolver<12> resolver;

  // Only register what you need; adjust to your model
  resolver.AddFullyConnected();
  resolver.AddConv2D();
  resolver.AddMaxPool2D();
  resolver.AddSoftmax();
  resolver.AddReshape();
  resolver.AddRelu();
  resolver.AddShape();
  resolver.AddStridedSlice();
  resolver.AddPack();
  resolver.AddExpandDims();
  resolver.AddMean();
  // (Avoid duplicate AddReshape: one call is enough)

  const tflite::Model* model_ptr = tflite::GetModel(model);
  if (model_ptr->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("TFLM: schema version mismatch");
    return false;
  }

  interpreter_ = new tflite::MicroInterpreter(
      model_ptr, resolver, arena_, kArenaSize, &micro_error_reporter);

  if (!interpreter_) {
    Serial.println("TFLM: failed to create interpreter");
    return false;
  }

  if (interpreter_->AllocateTensors() != kTfLiteOk) {
    Serial.println("TFLM: AllocateTensors failed (increase kArenaSize)");
    return false;
  }

  input_  = interpreter_->input(0);
  output_ = interpreter_->output(0);

  // Cache quant params (if int8)
  if (input_->type == kTfLiteInt8 && input_->quantization.type == kTfLiteAffineQuantization) {
    auto* q = (TfLiteAffineQuantization*) input_->quantization.params;
    input_scale_      = q->scale->data[0];
    input_zero_point_ = q->zero_point->data[0];
  }
  if (output_->type == kTfLiteInt8 && output_->quantization.type == kTfLiteAffineQuantization) {
    auto* q = (TfLiteAffineQuantization*) output_->quantization.params;
    output_scale_      = q->scale->data[0];
    output_zero_point_ = q->zero_point->data[0];
  }

  Serial.println("TFLM: ready");
  return true;
}

inline void GestureAI::quantizeAndStore_(
    int sample_idx, float ax, float ay, float az, float gx, float gy, float gz) {
  // Normalize to [0,1] using expected IMU ranges; tweak to your training ranges
  float axn = (ax + 4.0f)   / 8.0f;     // [-4g, +4g]
  float ayn = (ay + 4.0f)   / 8.0f;
  float azn = (az + 4.0f)   / 8.0f;
  float gxn = (gx + 2000.f) / 4000.f;   // [-2000 dps, +2000 dps]
  float gyn = (gy + 2000.f) / 4000.f;
  float gzn = (gz + 2000.f) / 4000.f;

  const int base = sample_idx * 6;
  if (input_->type == kTfLiteInt8) {
    auto q = [&](float v)->int8_t {
      float qv = (v / input_scale_) + input_zero_point_;
      // clamp to int8 range
      if (qv > 127.f) qv = 127.f;
      if (qv < -128.f) qv = -128.f;
      return (int8_t) lrintf(qv);
    };
    input_->data.int8[base + 0] = q(axn);
    input_->data.int8[base + 1] = q(ayn);
    input_->data.int8[base + 2] = q(azn);
    input_->data.int8[base + 3] = q(gxn);
    input_->data.int8[base + 4] = q(gyn);
    input_->data.int8[base + 5] = q(gzn);
  } else if (input_->type == kTfLiteFloat32) {
    input_->data.f[base + 0] = axn;
    input_->data.f[base + 1] = ayn;
    input_->data.f[base + 2] = azn;
    input_->data.f[base + 3] = gxn;
    input_->data.f[base + 4] = gyn;
    input_->data.f[base + 5] = gzn;
  }
}


bool GestureAI::runIfTriggered(IMU& imu, float* probs_out, int* top_idx_out) {
  // Quick, single-sample peek — no extra delay here.
  float ax, ay, az;
  if (!imu.readAccel(ax, ay, az)) return false;

  const float asum = fabsf(ax) + fabsf(ay) + fabsf(az);
  if (asum < GESTURE_ACCEL_START_THRESH) {
    // Not triggered; return immediately.
    return false;
  }

  // Triggered: do a full capture + inference (with your class’s own timing).
  return doCaptureAndInfer_(imu, probs_out, top_idx_out);
}

bool GestureAI::doCaptureAndInfer_(IMU& imu, float* probs_out, int* top_idx_out) {
  Serial.println("=== Gesture: capturing window ===");
  for (int i = 0; i < GESTURE_SAMPLES; ++i) {
    float ax, ay, az, gx, gy, gz;
    imu.readAll(ax, ay, az, gx, gy, gz);
    quantizeAndStore_(i, ax, ay, az, gx, gy, gz);
  }

  Serial.println("=== Gesture: running inference ===");
  if (interpreter_->Invoke() != kTfLiteOk) {
    Serial.println("TFLM: Invoke failed");
    return false;
  }

  int best_i = 0;
  float best_p = -1e9f;
  for (int i = 0; i < GESTURE_NUM_CLASSES; ++i) {
    float p = 0.f;
    if (output_->type == kTfLiteInt8) {
      const int8_t qv = output_->data.int8[i];
      p = (qv - output_zero_point_) * output_scale_;
    } else {
      p = output_->data.f[i];
    }
    if (probs_out) probs_out[i] = p;
    if (p > best_p) { best_p = p; best_i = i; }
  }
  if (top_idx_out) *top_idx_out = best_i;

  Serial.print("Top class: "); Serial.print(best_i);
  Serial.print("  score: "); Serial.println(best_p, 4);
  Serial.println("=== Gesture: done ===");
  return true;
}

