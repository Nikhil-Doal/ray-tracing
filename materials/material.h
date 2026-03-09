#pragma once
#include "../core/ray.h"
#include "../core/hittable.h"

class HitRecord;

class Material {
public:
  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const = 0;
};