#pragma once
#include "../core/vec3.h"

class Texture {
public:
  virtual Vec3 value(double u, double v, const Vec3 &point, double ray_t = 0.0) const = 0;
  virtual ~Texture() = default;
};