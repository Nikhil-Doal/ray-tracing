#pragma once
#include "vec3.h"

class Ray {
  public:
  Vec3 origin;
  Vec3 direction;
  Ray() {}
  Ray(const Vec3 &origin, const Vec3 &direction);
  Vec3 at(double t) const;
};
