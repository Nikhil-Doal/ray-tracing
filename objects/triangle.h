#pragma once
#include "../core/hittable.h"
#include <memory>

class Triangle : public Hittable {
public:
  Vec3 v0, v1, v2;
  std::shared_ptr<Material> mat;
  Triangle(Vec3 a, Vec3 b, Vec3 c, std::shared_ptr<Material> mat);
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};

