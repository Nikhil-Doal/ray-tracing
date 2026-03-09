#pragma once
#include "../core/hittable.h"

class Sphere : public Hittable {
public:
  Sphere(Vec3 c, double r, Material* m);
  Vec3 center;
  double radius;
  Material *mat;
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  
};
Sphere::Sphere(Vec3 c, double r, Material* m) : center(c), radius(r), mat(m) {}
bool Sphere::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  Vec3 oc = ray.origin - center;

  double a = ray.direction.dot(ray.direction);
  double b = oc.dot(ray.direction);
  double c = oc.dot(oc) - radius*radius;

  double discriminant = b*b - a*c;

  if (discriminant > 0) {
    double t = (-b - sqrt(discriminant)) / a;
    if (t > t_min && t < t_max) {
      rec.t = t;
      rec.mat = mat;
      rec.point = ray.at(t);
      
      Vec3 outward_normal = (rec.point - center) / radius;
      rec.set_face_normal(ray, outward_normal);
      return true;
    }
  }
  return false;
}
