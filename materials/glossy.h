#pragma once
#include "material.h"
#include "../textures/texture.h"
#include "../utils/sampling_utils.h"
#include <memory>
#include <cmath>

class Glossy : public Material {
public:
  std::shared_ptr<Texture> albedo;
  double roughness;    // 0 = mirror, 1 = fully rough
  double specular_strength;  // 0-1, how much specular vs diffuse

  Glossy(std::shared_ptr<Texture> a, double roughness = 0.3, double spec = 0.5);

  bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
  double scattering_pdf(const Ray &ray_in, const HitRecord &rec, const Ray &scattered) const override;
  Vec3 albedo_at(const HitRecord &rec) const override;
};