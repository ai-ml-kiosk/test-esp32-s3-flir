#include "thermal_simulator.h"

#include <math.h>

void ThermalSimulator::nextFrame(ThermalFrame& frame) {
  frame.lowTempC = 99.0f;
  frame.highTempC = -99.0f;

  const float t = frameCounter_ * 0.08f;
  const float cx = 40.0f + sinf(t * 0.72f) * 8.0f;
  const float cy = 30.0f + cosf(t * 0.55f) * 6.0f;
  const float hotX = 56.0f + sinf(t * 0.48f) * 5.0f;
  const float hotY = 22.0f + cosf(t * 0.60f) * 4.0f;
  const float coldX = 13.0f + cosf(t * 0.50f) * 4.0f;
  const float coldY = 14.0f + sinf(t * 0.45f) * 3.0f;

  for (int y = 0; y < kThermalFrameHeight; ++y) {
    for (int x = 0; x < kThermalFrameWidth; ++x) {
      const float dx = (x - cx) / 26.0f;
      const float dy = (y - cy) / 21.0f;
      const float body = expf(-(dx * dx + dy * dy)) * 11.5f;
      const float hdx = (x - hotX) / 8.0f;
      const float hdy = (y - hotY) / 7.0f;
      const float hot = expf(-(hdx * hdx + hdy * hdy)) * 5.0f;
      const float cdx = (x - coldX) / 6.0f;
      const float cdy = (y - coldY) / 6.0f;
      const float cold = expf(-(cdx * cdx + cdy * cdy)) * 5.0f;
      const float wave = sinf(x * 0.17f + t) * 0.35f + cosf(y * 0.21f - t) * 0.25f;
      const float temp = 24.0f + body + hot - cold + wave;

      frame.pixels[y][x] = temp;
      if (temp > frame.highTempC) {
        frame.highTempC = temp;
        frame.highX = x;
        frame.highY = y;
      }
      if (temp < frame.lowTempC) {
        frame.lowTempC = temp;
        frame.lowX = x;
        frame.lowY = y;
      }
    }
  }

  ++frameCounter_;
}
