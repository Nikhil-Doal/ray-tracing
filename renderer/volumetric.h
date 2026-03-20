#pragma once
#include "../core/vec3.h"
#include "../core/hittable.h"
#include "../core/ray.h"
#include "../materials/material.h"
#include "../lights/light.h"
#include "../utils/math_utils.h"
#include <vector>
#include <memory>

using LightList = std::vector<std::shared_ptr<Light>>;

// Henyey-Greenstein phase function
// g = 0 -> isotropic, g > 0 -> forward scatter (sun shaft), g < 0 -> back scatter
inline double hg_phase(double cos_theta, double g) {
  double g2 = g * g;
  double denom = 1.0 + g2 - 2.0 * g * cos_theta;
  return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

struct VolumeAABB {
  Vec3 mn, mx;
  bool contains(const Vec3 &p) const {
    return p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y && p.z >= mn.z && p.z <= mx.z;
  }
  bool intersect(const Ray &ray, double &t0, double &t1) const {
    t0 = 0.0; t1 = 1e30;
    for (int i = 0; i < 3; ++i) {
      double inv = 1.0 / ray.direction[i];
      double ta = (mn[i] - ray.origin[i]) * inv;
      double tb = (mx[i] - ray.origin[i]) * inv;
      if (ta > tb) std::swap(ta, tb);
      t0 = std::max(t0, ta);
      t1 = std::min(t1, tb);
      if (t0 >= t1) return false;
    }
    return t0 < t1;
  }
};

struct VolumeRegion {
  VolumeAABB bounds;
  double sigma_s; // scattering coefficient
  double sigma_a; // absorption coefficient
  double g; // HG anisotropy [-1,1]

  double sigma_t() const { return sigma_s + sigma_a; }
  double albedo() const { return sigma_t() > 0 ? sigma_s / sigma_t() : 0.0; }
};

// Integrate single-scattering god ray contribution along [t0, t1].
inline Vec3 integrate_volume(const Ray &ray, double t0, double t1, const VolumeRegion &vol, const Hittable &world, const LightList &lights, int n_steps = 32) {
  if (lights.empty()) return Vec3(0, 0, 0);
  if (vol.sigma_t() < 1e-9) return Vec3(0, 0, 0);

  double step = (t1 - t0) / n_steps;
  Vec3 result(0, 0, 0);
  double transmittance = 1.0;

  for (int i = 0; i < n_steps; ++i) {
    double t_mid = t0 + (i + 0.5) * step;
    Vec3 p = ray.at(t_mid);

    transmittance *= exp(-vol.sigma_t() * step);

    for (const auto &light : lights) {
      LightSample ls = light->sample(p);
      if (ls.pdf <= 0.0) continue;

      Vec3 to_light = ls.point - p;
      double dist = to_light.norm();
      Vec3 to_light_dir = to_light / dist;

      // shadow ray
      HitRecord shadow_rec;
      Ray shadow(p, to_light_dir);
      bool blocked = world.hit(shadow, 0.001, dist - 0.01, shadow_rec);
      if (blocked && !shadow_rec.mat->is_emissive()) continue;

      // phase function
      double cos_theta = ray.direction.normalize().dot(to_light_dir);
      double phase = hg_phase(cos_theta, vol.g);

      // AABB intersection to get the actual distance through the volume
      double trans_to_light = 1.0;
      {
        double tl0, tl1;
        if (vol.bounds.intersect(shadow, tl0, tl1)) {
          double vol_dist = std::min(tl1 - tl0, dist);
          trans_to_light = exp(-vol.sigma_t() * vol_dist);
        }
      }

      result = result + ls.emission * (transmittance * vol.sigma_s * phase * trans_to_light * step / ls.pdf);
    }
  }
  return result;
}

inline Vec3 volume_contribution(const Ray &ray, double t_surface, const VolumeRegion &vol, const Hittable &world, const LightList &lights, int n_steps = 32)
{
  double t0, t1;
  if (!vol.bounds.intersect(ray, t0, t1)) return Vec3(0, 0, 0);
  t1 = std::min(t1, t_surface);
  if (t1 <= t0) return Vec3(0, 0, 0);
  return integrate_volume(ray, t0, t1, vol, world, lights, n_steps);
}
