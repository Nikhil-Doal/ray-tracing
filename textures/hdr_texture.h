#pragma once
#include "texture.h"
extern "C" float *stbi_loadf(char const *filename, int *x, int *y, int *comp, int req_comp);
extern "C" void stbi_image_free(void *retval_from_stbi_load);
#include "../core/vec3.h"
#include <string>
#include <iostream>
#include <cmath>

class HdrTexture : public Texture {
public:
  float *data;
  int width, height, channels;

  HdrTexture(const std::string &filename) : data(nullptr), width(0), height(0), channels(0) {
    data = stbi_loadf(filename.c_str(), &width, &height, &channels, 3);
    if (!data) {
      std::cerr << "Failed to load HDR texture: " << filename << "\n";
      width = height = channels = 0;
    } else {
      std::cout << "Loaded HDR: " << filename << " (" << width << "x" << height << ")\n";
    }
  }

  ~HdrTexture() {
    if (data) stbi_image_free(data);
  }

  HdrTexture(const HdrTexture &) = delete;
  HdrTexture &operator=(const HdrTexture &) = delete;

  Vec3 value(double u, double v, const Vec3 &point, double ray_t = 0.0) const override {
    if (!data) return Vec3(0, 0, 0);

    u = u - floor(u);
    v = v - floor(v);
    v = 1.0 - v;  // flip V (stb origin = top-left)

    double fi = u * (width - 1);
    double fj = v * (height - 1);
    int i0 = (int)fi;
    int j0 = (int)fj;
    int i1 = (i0 + 1 < width)  ? i0 + 1 : i0;
    int j1 = (j0 + 1 < height) ? j0 + 1 : j0;
    double tx = fi - i0;
    double ty = fj - j0;

    auto get = [&](int ii, int jj) -> Vec3 {
      int idx = (jj * width + ii) * 3;
      return Vec3(data[idx], data[idx+1], data[idx+2]);
    };

    Vec3 c00 = get(i0, j0), c10 = get(i1, j0);
    Vec3 c01 = get(i0, j1), c11 = get(i1, j1);
    Vec3 c0 = c00 * (1 - tx) + c10 * tx;
    Vec3 c1 = c01 * (1 - tx) + c11 * tx;
    return c0 * (1 - ty) + c1 * ty;
  }
};