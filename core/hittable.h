#pragma once
#include "ray.h"
#include "../objects/aabb.h"

class Material;

struct HitRecord {
  Vec3 point;
  Vec3 normal;
  double t;

  double u;
  double v;

  Material *mat;
  bool front_face;

  inline void set_face_normal(const Ray &ray, const Vec3 &outward_normal) {
    front_face = ray.direction.dot(outward_normal) < 0;
    normal = front_face ? outward_normal : outward_normal * -1;
  }

};

class Hittable {
public:
  virtual bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const = 0;
  virtual bool bounding_box(AABB &output_box) const = 0;
};