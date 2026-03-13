#pragma once
#include "../core/hittable.h"
#include <cmath>

void get_sphere_uv(const Vec3 &p, double &u, double &v);

class Sphere : public Hittable {
public:
  Sphere(Vec3 c, double r, Material* m);
  Vec3 center;
  double radius;
  Material *mat;
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};

