#pragma once
#include "../core/hittable.h"

class Triangle : public Hittable {
public:
  Vec3 v0, v1, v2;
  Material *mat;
  Triangle(Vec3 a, Vec3 b, Vec3 c, Material *mat) : v0(a), v1(b), v2(c), mat(mat) {}
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};

