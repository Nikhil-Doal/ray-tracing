#pragma once
#include "material.h"

class Dielectric : public Material {
public:
  double ir; // the index of refraction
  Dielectric(double ir) : ir(ir) {}
  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
};

bool Dielectric::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const{
  
  attenuation = Vec3(1.0, 1.0, 1.0);
  double refract_ratio = rec.front_face ? (1.0/ir) : ir;
  Vec3 unit_dir = ray_in.direction.normalize();
  Vec3 direction = refract(unit_dir, rec.normal, refract_ratio);
  scattered = Ray(rec.point, direction);

  return true;
}

// helpers
// using schlick's approximation to approximate the fresnel equations
double reflectance(double cosine, double ref_idx) {
  double r0 = (1 - ref_idx) / (1 + ref_idx);
  r0 = r0 * r0;
  return r0+ (1 - r0) * pow((1 - cosine), 5);
}