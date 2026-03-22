#pragma once
#include "texture.h"
#include "../external/stbimagewrite/stb_image.h"
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
    v = 1.0 - v;
    return sample_bilinear(u, v);
  }

  Vec3 sky_sample(const Vec3 &direction) const {
    if (!data) return Vec3(0, 0, 0);
    Vec3 unit = direction.normalize();
    double theta = acos(fmax(-1.0, fmin(1.0, unit.y)));
    double phi = atan2(unit.z, unit.x) + PI;
    double u = phi / (2.0 * PI);
    double v = theta / PI;
    v = 1.0 - v;  // single flip for stb top-left origin
    return sample_bilinear(u, v);
  }

private:
// Bilinear sample at (u,v) in [0,1], v=0 is top row (stb convention). No extra flipping.
  Vec3 sample_bilinear(double u, double v) const {
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