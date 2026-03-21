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
inline double hg_phase(double cos_theta, double g)
{
  double g2 = g * g;
  double denom = 1.0 + g2 - 2.0 * g * cos_theta;
  return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

struct VolumeAABB
{
  Vec3 mn, mx;
  bool contains(const Vec3 &p) const
  {
    return p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y && p.z >= mn.z && p.z <= mx.z;
  }

  bool intersect(const Ray &ray, double &t0, double &t1) const
  {
    t0 = 0.0;
    t1 = 1e30;
    for (int i = 0; i < 3; ++i)
    {
      double inv = 1.0 / ray.direction[i];
      double ta = (mn[i] - ray.origin[i]) * inv;
      double tb = (mx[i] - ray.origin[i]) * inv;
      if (ta > tb)
        std::swap(ta, tb);
      t0 = std::max(t0, ta);
      t1 = std::min(t1, tb);
      if (t0 >= t1)
        return false;
    }
    return t0 < t1;
  }
};

struct VolumeRegion
{
  VolumeAABB bounds;
  double sigma_s; // scattering coefficient
  double sigma_a; // absorption coefficient
  double g;       // HG anisotropy [-1,1]

  double sigma_t() const { return sigma_s + sigma_a; }
  double albedo() const { return sigma_t() > 0 ? sigma_s / sigma_t() : 0.0; }
};

// Stochastic single-scatter volumetric integration
// (O(n_steps × n_lights × BVH) per ray) to O(1), fire one scattering ray with delta tracking
inline Vec3 integrate_volume_stochastic(const Ray &ray, double t0, double t1, const VolumeRegion &vol, const Hittable &world, const LightList &lights)
{
  if (lights.empty() || vol.sigma_t() < 1e-9)
    return Vec3(0, 0, 0);
  double seg_len = t1 - t0;
  if (seg_len < 1e-6)
    return Vec3(0, 0, 0);

  // Beer Lambert -> P(scattering event in [t0, t1])
  double p_scatter = 1.0 - exp(-vol.sigma_t() * seg_len);
  if (random_double() > p_scatter)
    return Vec3(0, 0, 0);

  // sample scattering position along the segment: t_scatter = -log(1-u*p)/sigma_t + t0
  double t_scatter = t0 - log(1.0 - random_double() * p_scatter) / vol.sigma_t();
  t_scatter = std::min(t_scatter, t1 - 1e-6);

  // transmittance from ray origin to scatter point
  Vec3 p = ray.at(t_scatter);
  double trans_to_p = exp(-vol.sigma_t() * (t_scatter - t0));

  // pick one random light to estimate direct in scatter
  int li = int(random_double() * lights.size());
  li = std::min(li, (int)lights.size() - 1);
  const auto &light = lights[li];

  LightSample ls = light->sample(p);
  if (ls.pdf <= 0.0)
    return Vec3(0, 0, 0);

  Vec3 to_light = ls.point - p;
  double dist = to_light.norm();
  Vec3 to_light_dir = to_light / dist;

  // Single shadow ray for the scatter point
  HitRecord shadow_rec;
  Ray shadow(p, to_light_dir);
  bool blocked = world.hit(shadow, 0.001, dist - 0.01, shadow_rec);
  if (blocked && shadow_rec.mat && !shadow_rec.mat->is_emissive())
    return Vec3(0, 0, 0);

  // Phase function
  double cos_theta = ray.direction.normalize().dot(to_light_dir);
  double phase = hg_phase(cos_theta, vol.g);

  // Volume transmittance to light through the volume AABB
  double trans_to_light = 1.0;
  {
    double vt0, vt1;
    if (vol.bounds.intersect(shadow, vt0, vt1))
    {
      double vol_dist = std::min(vt1 - vt0, dist);
      trans_to_light = exp(-vol.sigma_t() * vol_dist);
    }
  }

  // Multi-light balance: weight by number of lights
  double light_weight = (double)lights.size();

  return ls.emission * (trans_to_p * vol.sigma_s * phase * trans_to_light / ls.pdf * light_weight);
}

inline Vec3 volume_contribution(const Ray &ray, double t_surface, const VolumeRegion &vol, const Hittable &world, const LightList &lights, int n_steps = 32)
{
  double t0, t1;
  if (!vol.bounds.intersect(ray, t0, t1))
    return Vec3(0, 0, 0);
  t1 = std::min(t1, t_surface);
  if (t1 <= t0)
    return Vec3(0, 0, 0);
  return integrate_volume_stochastic(ray, t0, t1, vol, world, lights);
}
