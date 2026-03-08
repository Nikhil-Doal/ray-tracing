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
Ray::Ray(const Vec3 &origin, const Vec3 &direction) : origin(origin), direction(direction) {}
Vec3 Ray::at(double t) const {
  return origin + direction * t;
}
