#pragma once
#include "../core/hittable.h"
#include "../materials/material.h"
#include <cmath>
#include <memory>

void get_sphere_uv(const Vec3 &p, double &u, double &v);

class Sphere : public Hittable {
public:
  Sphere(Vec3 c, double r, std::shared_ptr<Material> m);
  Vec3 center;
  double radius;
  std::shared_ptr<Material> mat;
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};

