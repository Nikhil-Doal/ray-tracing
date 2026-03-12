#pragma once
#include "material.h"
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"

class Metal : public Material {
public:
  Texture* albedo;
  double fuzz;
  Metal(Texture* a, double f) : albedo(a), fuzz(f < 1 ? f : 1) {}

  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
};

bool Metal::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const {
  Vec3 reflected = reflect(ray_in.direction.normalize(), rec.normal);

  scattered = Ray(rec.point, reflected + random_in_unit_sphere() * fuzz);
  attenuation = albedo->value(rec.u, rec.v, rec.point);

  return scattered.direction.dot(rec.normal) > 0;
}
