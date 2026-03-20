#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include "../external/stbimagewrite/stb_image_write.h"
#include "../core/vec3.h"

// Save a framebuffer (Vec3 RGB) to a PNG
inline bool save_png(const std::string &filename, int width, int height, const std::vector<Vec3> &framebuffer, int samples_per_pixel, double exposure = 1.0) {
  std::vector<uint8_t> image_data(width * height * 3);

  auto clamp = [](double x, double min, double max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
  };

  for (int j = 0; j < height; ++j) {
    int flipped_j = height - 1 - j;
    for (int i = 0; i < width; ++i) {
      Vec3 pixel_color = framebuffer[flipped_j * width + i] / samples_per_pixel;

      // Apply exposure before tonemapping
      pixel_color = pixel_color * exposure;

      // reinhard tonemapping
      pixel_color = Vec3(pixel_color.x / (1.0 + pixel_color.x), pixel_color.y / (1.0 + pixel_color.y), pixel_color.z / (1.0 + pixel_color.z));
      // Gamma correction
      pixel_color = Vec3(sqrt(pixel_color.x), sqrt(pixel_color.y), sqrt(pixel_color.z));

      int index = (j * width + i) * 3;
      image_data[index + 0] = static_cast<uint8_t>(256 * clamp(pixel_color.x, 0.0, 0.999));
      image_data[index + 1] = static_cast<uint8_t>(256 * clamp(pixel_color.y, 0.0, 0.999));
      image_data[index + 2] = static_cast<uint8_t>(256 * clamp(pixel_color.z, 0.0, 0.999));
    }
  }

  return stbi_write_png(filename.c_str(), width, height, 3, image_data.data(), width * 3) != 0;
}