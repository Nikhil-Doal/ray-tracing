#pragma once
#include "material.h"
#include "../textures/texture.h"
#include "../utils/sampling_utils.h"
#include <memory>
#include <cmath>

// Glossy material used for normal maps -> diffuse from lambertian + specular
class Glossy : public Material {
public:
  std::shared_ptr<Texture> albedo;
  double roughness;    // 0 = mirror, 1 = fully rough
  double specular_strength;  // 0-1, how much specular vs diffuse

  Glossy(std::shared_ptr<Texture> a, double roughness = 0.3, double spec = 0.1)
    : albedo(a), roughness(roughness), specular_strength(spec) {}

  bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override {
    // Probabilistically choose diffuse or specular based on specular_strength
    bool do_specular = (random_double() < specular_strength);

    if (do_specular) {
      // GGX importance sampling around the reflected direction
      Vec3 unit_in = ray_in.direction.normalize();
      Vec3 reflected = reflect(unit_in, rec.normal);

      // Sample GGX lobe: perturb the reflected direction by roughness
      ONB onb;
      onb.build_from_w(reflected);
      
      double r1 = random_double();
      double r2 = random_double();
      double a2 = roughness * roughness;
      // GGX theta sampling: theta = atan(a * sqrt(r1) / sqrt(1-r1))
      double cos_theta = sqrt((1.0 - r1) / (1.0 + (a2 * a2 - 1.0) * r1));
      double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
      double phi = 2.0 * PI * r2;
      
      Vec3 half_vec = onb.local(Vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta)).normalize();

      // Reflect incoming around the half vector
      Vec3 spec_dir = reflect(unit_in, half_vec);
      
      if (spec_dir.dot(rec.normal) <= 0) {
        // Below surface, fall back to diffuse
        ONB diffuse_onb;
        diffuse_onb.build_from_w(rec.normal);
        spec_dir = diffuse_onb.local(random_cosine_direction());
      }

      scattered = Ray(rec.point, spec_dir);
      // Fresnel-modulated specular color (simple Schlick with F0=0.04 for dielectric)
      double cos_i = fmax(0.0, rec.normal.dot(spec_dir.normalize()));
      double f0 = 0.04;
      double fresnel = f0 + (1.0 - f0) * pow(1.0 - cos_i, 5.0);
      attenuation = Vec3(fresnel, fresnel, fresnel);
    } else {
      // Diffuse (Lambertian)
      ONB onb;
      onb.build_from_w(rec.normal);
      Vec3 scatter_direction = onb.local(random_cosine_direction());
      scattered = Ray(rec.point, scatter_direction);
      attenuation = albedo->value(rec.u, rec.v, rec.point, rec.t);
    }
    return true;
  }

  double scattering_pdf(const Ray &ray_in, const HitRecord &rec, const Ray &scattered) const override {
    // Mixed PDF: combine diffuse cosine PDF with specular lobe
    double cos_theta = rec.normal.dot(scattered.direction.normalize());
    double diffuse_pdf = cos_theta < 0 ? 0.0 : cos_theta / PI;
    // For simplicity, use the diffuse pdf (MIS will handle the rest)
    return diffuse_pdf * (1.0 - specular_strength) + 0.001; // small floor to avoid div by zero
  }

  Vec3 albedo_at(const HitRecord &rec) const override {
    return albedo->value(rec.u, rec.v, rec.point, rec.t);
  }
};