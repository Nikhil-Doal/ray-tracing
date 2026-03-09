#pragma once
#include "ray.h"
#include "../materials/material.h"

struct HitRecord {
  Vec3 point;
  Vec3 normal;
  double t;
  Material *mat;
};

class Hittable {
public:
  virtual bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const = 0;
};