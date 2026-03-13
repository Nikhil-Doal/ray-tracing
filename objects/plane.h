#pragma once
#include "../core/hittable.h"
#include <memory>

class Plane : public Hittable {
public:
  Vec3 point;
  Vec3 normal;
  std::shared_ptr<Material> mat;
  double side;

  Plane(const Vec3 &p, const Vec3 &n, std::shared_ptr<Material> m, double s = 0.0);
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};