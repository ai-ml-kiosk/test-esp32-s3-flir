#include "flir/image_processing.h"

namespace flir {
namespace {

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint8_t lerp8(uint8_t a, uint8_t b, uint8_t amount) {
  return a + (((int16_t)b - a) * amount) / 255;
}

uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount) {
  const uint8_t ar = ((a >> 11) & 0x1F) << 3;
  const uint8_t ag = ((a >> 5) & 0x3F) << 2;
  const uint8_t ab = (a & 0x1F) << 3;
  const uint8_t br = ((b >> 11) & 0x1F) << 3;
  const uint8_t bg = ((b >> 5) & 0x3F) << 2;
  const uint8_t bb = (b & 0x1F) << 3;
  return rgb565(lerp8(ar, br, amount), lerp8(ag, bg, amount), lerp8(ab, bb, amount));
}

}  // namespace

bool ImageProcessor::isFrameUsable(const uint16_t* raw14,
                                   size_t pixelCount,
                                   uint16_t* minRaw,
                                   uint16_t* maxRaw) const {
  if (raw14 == nullptr || pixelCount == 0) {
    Serial.println("Frame quality error: null or empty raw frame");
    return false;
  }

  uint16_t low = 0xFFFF;
  uint16_t high = 0;
  uint32_t zeroCount = 0;
  uint32_t saturatedCount = 0;
  for (size_t i = 0; i < pixelCount; ++i) {
    const uint16_t value = raw14[i];
    low = min(low, value);
    high = max(high, value);
    if (value == 0) {
      ++zeroCount;
    } else if (value == 0xFFFF) {
      ++saturatedCount;
    }
  }

  if (minRaw != nullptr) {
    *minRaw = low;
  }
  if (maxRaw != nullptr) {
    *maxRaw = high;
  }

  const uint16_t span = high - low;
  const uint32_t badLimit = pixelCount / 8;
  if (span < 16 || zeroCount > badLimit || saturatedCount > badLimit) {
    Serial.printf("Frame quality reject: low=%u high=%u span=%u zero=%lu sat=%lu\n",
                  low,
                  high,
                  span,
                  static_cast<unsigned long>(zeroCount),
                  static_cast<unsigned long>(saturatedCount));
    return false;
  }
  return true;
}

bool ImageProcessor::automaticGainControl(const uint16_t* raw14,
                                          size_t pixelCount,
                                          uint8_t* out8,
                                          uint16_t* minRaw,
                                          uint16_t* maxRaw) const {
  if (raw14 == nullptr || out8 == nullptr || pixelCount == 0) {
    Serial.println("AGC error: null buffer or empty frame");
    return false;
  }

  static bool hasSmoothedWindow = false;
  static uint16_t smoothedLow = 0;
  static uint16_t smoothedHigh = 0;

  uint16_t trueLow = 0x3FFF;
  uint16_t trueHigh = 0;
  uint16_t histogram[256] = {};
  for (size_t i = 0; i < pixelCount; ++i) {
    const uint16_t value = raw14[i] & 0x3FFF;
    trueLow = min(trueLow, value);
    trueHigh = max(trueHigh, value);
    ++histogram[value >> 6];
  }

  const uint32_t lowCutoff = max<uint32_t>(1, pixelCount / 100);
  const uint32_t highCutoff = max<uint32_t>(1, pixelCount - (pixelCount / 100));
  uint32_t cumulative = 0;
  uint16_t low = trueLow;
  uint16_t high = trueHigh;

  for (uint16_t bucket = 0; bucket < 256; ++bucket) {
    cumulative += histogram[bucket];
    if (cumulative >= lowCutoff) {
      low = bucket << 6;
      break;
    }
  }

  cumulative = 0;
  for (uint16_t bucket = 0; bucket < 256; ++bucket) {
    cumulative += histogram[bucket];
    if (cumulative >= highCutoff) {
      high = min<uint16_t>(0x3FFF, (bucket << 6) | 0x3F);
      break;
    }
  }

  if (high <= low || high - low < 32) {
    low = trueLow;
    high = trueHigh;
  }

  if (!hasSmoothedWindow) {
    smoothedLow = low;
    smoothedHigh = high;
    hasSmoothedWindow = true;
  } else {
    smoothedLow = static_cast<uint16_t>((static_cast<uint32_t>(smoothedLow) * 7U + low) / 8U);
    smoothedHigh = static_cast<uint16_t>((static_cast<uint32_t>(smoothedHigh) * 7U + high) / 8U);
  }

  if (smoothedHigh <= smoothedLow || smoothedHigh - smoothedLow < 32) {
    smoothedLow = low;
    smoothedHigh = high;
  }

  if (minRaw != nullptr) {
    *minRaw = trueLow;
  }
  if (maxRaw != nullptr) {
    *maxRaw = trueHigh;
  }

  const uint16_t span = smoothedHigh - smoothedLow;
  if (span == 0) {
    memset(out8, 0, pixelCount);
    return true;
  }

  for (size_t i = 0; i < pixelCount; ++i) {
    const uint16_t value = raw14[i] & 0x3FFF;
    if (value <= smoothedLow) {
      out8[i] = 0;
    } else if (value >= smoothedHigh) {
      out8[i] = 255;
    } else {
      out8[i] = static_cast<uint8_t>((static_cast<uint32_t>(value - smoothedLow) * 255U) / span);
    }
  }
  return true;
}

bool ImageProcessor::histogramEqualizeRaw14(const uint16_t* raw14,
                                            size_t pixelCount,
                                            uint8_t* out8,
                                            uint16_t* minRaw,
                                            uint16_t* maxRaw) const {
  if (raw14 == nullptr || out8 == nullptr || pixelCount == 0) {
    Serial.println("HEQ error: null buffer or empty frame");
    return false;
  }

  static bool hasPreviousLut = false;
  static uint8_t previousLut[256] = {};

  uint16_t low = 0xFFFF;
  uint16_t high = 0;
  for (size_t i = 0; i < pixelCount; ++i) {
    const uint16_t value = raw14[i];
    low = min(low, value);
    high = max(high, value);
  }

  if (minRaw != nullptr) {
    *minRaw = low;
  }
  if (maxRaw != nullptr) {
    *maxRaw = high;
  }

  const uint16_t span = high - low;
  if (span == 0) {
    memset(out8, 0, pixelCount);
    return true;
  }

  uint16_t histogram[256] = {};
  for (size_t i = 0; i < pixelCount; ++i) {
    const uint16_t value = raw14[i];
    const uint8_t bucket = static_cast<uint8_t>((static_cast<uint32_t>(value - low) * 255U) / span);
    ++histogram[bucket];
  }

  // Contrast-limited HEQ: map the current 16-bit TLinear frame into 256
  // dynamic buckets, then clip before building the CDF so a noisy flat
  // background cannot consume the whole output range.
  const uint16_t clipLimit = max<uint16_t>(8, pixelCount / 24);
  uint32_t clipped = 0;
  for (uint16_t i = 0; i < 256; ++i) {
    if (histogram[i] > clipLimit) {
      clipped += histogram[i] - clipLimit;
      histogram[i] = clipLimit;
    }
  }

  const uint16_t redistribute = clipped / 256;
  const uint16_t remainder = clipped % 256;
  for (uint16_t i = 0; i < 256; ++i) {
    histogram[i] += redistribute + (i < remainder ? 1 : 0);
  }

  uint32_t cdf = 0;
  uint32_t cdfMin = 0;
  uint8_t lut[256] = {};
  for (uint16_t i = 0; i < 256; ++i) {
    cdf += histogram[i];
    if (cdfMin == 0 && cdf != 0) {
      cdfMin = cdf;
    }

    if (cdf <= cdfMin || pixelCount <= cdfMin) {
      lut[i] = 0;
    } else {
      // The CDF maps rank order to luminance. Integer math keeps this cheap on
      // ESP32-S3 while preserving fine contrast in dense thermal scenes.
      lut[i] = static_cast<uint8_t>(((cdf - cdfMin) * 255U) / (pixelCount - cdfMin));
    }
  }

  if (hasPreviousLut) {
    for (uint16_t i = 0; i < 256; ++i) {
      lut[i] = static_cast<uint8_t>((static_cast<uint16_t>(previousLut[i]) + (static_cast<uint16_t>(lut[i]) * 3U)) /
                                    4U);
      previousLut[i] = lut[i];
    }
  } else {
    memcpy(previousLut, lut, sizeof(lut));
    hasPreviousLut = true;
  }

  for (size_t i = 0; i < pixelCount; ++i) {
    const uint8_t bucket = static_cast<uint8_t>((static_cast<uint32_t>(raw14[i] - low) * 255U) / span);
    out8[i] = lut[bucket];
  }
  return true;
}

bool ImageProcessor::upscaleBilinear(const uint8_t* src,
                                     uint16_t srcWidth,
                                     uint16_t srcHeight,
                                     uint8_t* dst,
                                     uint16_t dstWidth,
                                     uint16_t dstHeight) const {
  if (src == nullptr || dst == nullptr || srcWidth < 2 || srcHeight < 2 || dstWidth == 0 || dstHeight == 0) {
    Serial.println("Upscale error: invalid source or destination buffer");
    return false;
  }

  const uint32_t xRatio = ((static_cast<uint32_t>(srcWidth - 1)) << 16) / max<uint16_t>(1, dstWidth - 1);
  const uint32_t yRatio = ((static_cast<uint32_t>(srcHeight - 1)) << 16) / max<uint16_t>(1, dstHeight - 1);

  for (uint16_t y = 0; y < dstHeight; ++y) {
    const uint32_t srcY = static_cast<uint32_t>(y) * yRatio;
    const uint16_t y0 = srcY >> 16;
    const uint16_t y1 = min<uint16_t>(y0 + 1, srcHeight - 1);
    const uint8_t yWeight = (srcY & 0xFFFF) >> 8;

    for (uint16_t x = 0; x < dstWidth; ++x) {
      const uint32_t srcX = static_cast<uint32_t>(x) * xRatio;
      const uint16_t x0 = srcX >> 16;
      const uint16_t x1 = min<uint16_t>(x0 + 1, srcWidth - 1);
      const uint8_t xWeight = (srcX & 0xFFFF) >> 8;

      // Bilinear interpolation blends the four neighboring source pixels. The
      // fractional source coordinate is held in 16.16 fixed point, then reduced
      // to an 8-bit blend weight so every inner-loop operation is integer-only.
      const uint8_t top = lerp8(src[y0 * srcWidth + x0], src[y0 * srcWidth + x1], xWeight);
      const uint8_t bottom = lerp8(src[y1 * srcWidth + x0], src[y1 * srcWidth + x1], xWeight);
      dst[static_cast<size_t>(y) * dstWidth + x] = lerp8(top, bottom, yWeight);
    }
  }
  return true;
}

bool ImageProcessor::denoise3x3(const uint8_t* src,
                                uint8_t* dst,
                                uint16_t width,
                                uint16_t height) const {
  if (src == nullptr || dst == nullptr || width < 3 || height < 3) {
    Serial.println("Denoise error: invalid source or destination buffer");
    return false;
  }

  memcpy(dst, src, static_cast<size_t>(width) * height);
  for (uint16_t y = 1; y < height - 1; ++y) {
    for (uint16_t x = 1; x < width - 1; ++x) {
      const size_t i = static_cast<size_t>(y) * width + x;
      const uint16_t sum =
          static_cast<uint16_t>(src[i] * 4U) +
          static_cast<uint16_t>(src[i - 1] * 2U) +
          static_cast<uint16_t>(src[i + 1] * 2U) +
          static_cast<uint16_t>(src[i - width] * 2U) +
          static_cast<uint16_t>(src[i + width] * 2U) +
          src[i - width - 1] +
          src[i - width + 1] +
          src[i + width - 1] +
          src[i + width + 1];
      dst[i] = static_cast<uint8_t>(sum / 16U);
    }
  }
  return true;
}

bool ImageProcessor::temporalSmooth(uint8_t* frame, size_t pixelCount, uint8_t previousWeight) const {
  if (frame == nullptr || pixelCount == 0 || pixelCount > kLeptonPixels) {
    Serial.println("Temporal smooth error: invalid frame buffer");
    return false;
  }

  static bool hasPrevious = false;
  static uint8_t previous[kLeptonPixels] = {};
  if (!hasPrevious) {
    memcpy(previous, frame, pixelCount);
    hasPrevious = true;
    return true;
  }

  previousWeight = min<uint8_t>(previousWeight, 7);
  const uint8_t currentWeight = 8 - previousWeight;
  for (size_t i = 0; i < pixelCount; ++i) {
    const uint8_t value = static_cast<uint8_t>(((static_cast<uint16_t>(previous[i]) * previousWeight) +
                                                (static_cast<uint16_t>(frame[i]) * currentWeight)) /
                                               8U);
    previous[i] = value;
    frame[i] = value;
  }
  return true;
}

bool ImageProcessor::sharpen3x3(uint8_t* frame, uint16_t width, uint16_t height, uint8_t amount) const {
  if (frame == nullptr || width < 3 || height < 3) {
    Serial.println("Sharpen error: invalid frame buffer");
    return false;
  }
  amount = min<uint8_t>(amount, 2);
  if (amount == 0) {
    return true;
  }

  static uint8_t scratch[kThermalViewPixels] = {};
  const size_t pixelCount = static_cast<size_t>(width) * height;
  if (pixelCount > kThermalViewPixels) {
    Serial.println("Sharpen error: frame is larger than scratch buffer");
    return false;
  }

  memcpy(scratch, frame, pixelCount);
  for (uint16_t y = 1; y < height - 1; ++y) {
    for (uint16_t x = 1; x < width - 1; ++x) {
      const size_t i = static_cast<size_t>(y) * width + x;
      const uint16_t blur = (static_cast<uint16_t>(scratch[i - 1]) + scratch[i + 1] + scratch[i - width] +
                             scratch[i + width]) /
                            4U;
      const int16_t detail = static_cast<int16_t>(scratch[i]) - static_cast<int16_t>(blur);
      const int16_t sharpened = static_cast<int16_t>(scratch[i]) + detail * amount;
      frame[i] = static_cast<uint8_t>(constrain(sharpened, 0, 255));
    }
  }
  return true;
}

void ImageProcessor::applyPalette(const uint8_t* normalized,
                                  size_t pixelCount,
                                  uint16_t* rgb565Out,
                                  PaletteMode palette) const {
  if (normalized == nullptr || rgb565Out == nullptr || pixelCount == 0) {
    Serial.println("Palette error: invalid buffer");
    return;
  }

  for (size_t i = 0; i < pixelCount; ++i) {
    switch (palette) {
      case PaletteMode::Ironbow:
        rgb565Out[i] = ironbow(normalized[i]);
        break;
      case PaletteMode::BlackHot:
        rgb565Out[i] = grayscale(normalized[i], true);
        break;
	      case PaletteMode::WhiteHot:
	      case PaletteMode::Histogram:
	      default:
	        rgb565Out[i] = grayscale(normalized[i], false);
	        break;
    }
  }
}

SpotTemperatures ImageProcessor::calculateSpotTemperatures(const uint16_t* raw14,
                                                           uint16_t width,
                                                           uint16_t height) const {
  SpotTemperatures spots;
  if (raw14 == nullptr || width == 0 || height == 0) {
    Serial.println("Spot meter error: invalid raw frame");
    return spots;
  }

  spots.minRaw = 0xFFFF;
  spots.maxRaw = 0;
  spots.minC = 1000.0f;
  spots.maxC = -1000.0f;
  auto average3x3 = [raw14, width](uint16_t x, uint16_t y) {
    uint32_t sum = 0;
    for (int8_t dy = -1; dy <= 1; ++dy) {
      for (int8_t dx = -1; dx <= 1; ++dx) {
        sum += raw14[static_cast<size_t>(y + dy) * width + (x + dx)];
      }
    }
    return static_cast<uint16_t>((sum + 4) / 9);
  };

  for (uint16_t y = 1; y < height - 1; ++y) {
    for (uint16_t x = 1; x < width - 1; ++x) {
      const size_t index = static_cast<size_t>(y) * width + x;
      (void)index;

      // TLinear radiometric mode stores full 16-bit centikelvin values. Do not
      // mask with 0x3FFF here; that corrupts normal room-temperature values.
      const uint16_t cleanValue = average3x3(x, y);

      // TLinear radiometric mode reports absolute temperature in centikelvin:
      // 29315 -> 293.15 K -> 20.0 C.
      const float temperatureCelsius = rawCentikelvinToCelsius(cleanValue);

      if (temperatureCelsius < spots.minC) {
        spots.minC = temperatureCelsius;
        spots.minRaw = cleanValue;
        spots.minX = x;
        spots.minY = y;
      }
      if (temperatureCelsius > spots.maxC) {
        spots.maxC = temperatureCelsius;
        spots.maxRaw = cleanValue;
        spots.maxX = x;
        spots.maxY = y;
      }
    }
  }

  const uint16_t centerX = width / 2;
  const uint16_t centerY = height / 2;
  spots.centerRaw = average3x3(centerX, centerY);
  spots.centerC = rawCentikelvinToCelsius(spots.centerRaw);
  return spots;
}

float ImageProcessor::rawCentikelvinToCelsius(uint16_t raw14) const {
  return (static_cast<float>(raw14) / 100.0f) - 273.15f;
}

uint16_t ImageProcessor::ironbow(uint8_t value) const {
  static constexpr uint16_t stops[] = {
      0x0000,  // black
      0x180F,  // deep violet
      0x481F,  // purple
      0xF800,  // red
      0xFD20,  // orange
      0xFFE0,  // yellow
      0xFFFF,  // white
  };
  const uint8_t segmentCount = sizeof(stops) / sizeof(stops[0]) - 1;
  const uint16_t scaled = static_cast<uint16_t>(value) * segmentCount;
  const uint8_t index = min<uint8_t>(segmentCount - 1, scaled / 255);
  const uint8_t amount = scaled - (index * 255);
  return blend565(stops[index], stops[index + 1], amount);
}

uint16_t ImageProcessor::grayscale(uint8_t value, bool blackHot) const {
  const uint8_t gray = blackHot ? 255 - value : value;
  return rgb565(gray, gray, gray);
}

}  // namespace flir
