#pragma once
#include "../core/hittable.h"

class Plane : public Hittable {
public:
  Vec3 point;
  Vec3 normal;
  Material *mat;
  double side;

  Plane(const Vec3 &p, const Vec3 &n, Material *m, double s = 0.0);
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};