#pragma once 
#include "../core/vec3.h"
#include "../core/ray.h"
#include <memory>

struct LightSample {
  Vec3 point; //pt on light surface
  Vec3 normal; 
  Vec3 emission;
  double pdf; // probability if this sample
};

class Light {
public:
  virtual LightSample sample(const Vec3 &from) const = 0;
  virtual Vec3 emission(const Vec3 &from, const Vec3 &dir) const = 0;
  virtual double pdf(const Vec3 &from, const Vec3 &dir) const = 0;
  virtual ~Light() = default;
};