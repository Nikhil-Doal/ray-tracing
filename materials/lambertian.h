// Working with the lambertian reflectance
#pragma once
#include "material.h"
#include "../textures/texture.h"
#include "../utils/sampling_utils.h"
#include <cstdlib>
#include <memory>

class Lambertian : public Material {
public:
  std::shared_ptr<Texture> albedo;
  Lambertian (std::shared_ptr<Texture> a);
  bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
  double scattering_pdf(const Ray &ray_in, const HitRecord &rec, const Ray &scattered) const override {
    double cos_theta = rec.normal.dot(scattered.direction.normalize());
    return cos_theta < 0 ? 0.0 : cos_theta / PI;
  }
  Vec3 albedo_at(const HitRecord &rec) const override {
    return albedo->value(rec.u, rec.v, rec.point, rec.t);
  }
};
