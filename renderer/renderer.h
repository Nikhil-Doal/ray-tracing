#pragma once
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include "../core/camera.h"
#include "../core/hittable.h"
#include "../lights/light.h"
#include "../textures/image_texture.h"
#include "../textures/texture.h"
#include "volumetric.h"

inline std::shared_ptr<Texture> g_sky_texture = nullptr;
inline float g_sky_intensity = 1.0f;

inline std::shared_ptr<VolumeRegion> g_volume = nullptr;

using LightList = std::vector<std::shared_ptr<Light>>;

struct Tile { int x, y, w, h; };

inline double power_heuristic(double pdf_a, double pdf_b) {
  double a2 = pdf_a * pdf_a;
  double b2 = pdf_b * pdf_b;
  return (a2 + b2) < 1e-6 ? 0.0 : a2 / (a2 + b2);
}

// Accumulates Beer-Lambert attenuation through each glass surface
inline bool trace_shadow(const Ray &origin_ray, const Hittable &world, double dist, Vec3 &shadow_atten) {
  shadow_atten = Vec3(1, 1, 1);
  Ray ray = origin_ray;
  double remaining = dist - 0.01;

  for (int i = 0; i < 16 && remaining > 0.001; ++i) {
    HitRecord shadow_rec;
    if (!world.hit(ray, 0.001, remaining, shadow_rec)) return true; // reached the light unblocked
    if (shadow_rec.mat->is_emissive()) return true; // hit the light itself
    if (shadow_rec.mat->is_transmissive()) {
      // glass — attenuate by material color and continue
      Vec3 mat_color = shadow_rec.mat->albedo_at(shadow_rec);
      shadow_atten = shadow_atten * mat_color;
      // advance past this surface
      ray = Ray(shadow_rec.point + ray.direction.normalize() * 0.002, ray.direction);
      remaining -= shadow_rec.t;
      continue;
    }
    // opaque, non-emissive — blocked
    return false;
  }
  return true;
}

inline Vec3 direct_light(const Ray &ray_in, const HitRecord &rec, const Hittable &world, const LightList &lights) {
  Vec3 direct(0, 0, 0);
  if (lights.empty()) return direct;
  // sampling all lights and accumulating
  for (const auto &light : lights) {
    // NEE sample
    LightSample ls = light->sample(rec.point);
    if (ls.pdf <= 0) continue;

    Vec3 to_light = ls.point - rec.point;
    double dist = to_light.norm();
    Vec3 to_light_dir = to_light / dist;

    double cos_theta = rec.normal.dot(to_light_dir);
    if (cos_theta <= 0) continue;

    // shadow ray
    Vec3 shadow_atten;
    Ray shadow_ray(rec.point, to_light_dir);
    if (!trace_shadow(shadow_ray, world, dist, shadow_atten)) continue;

    // get surface attenuation
    Vec3 attenuation = rec.mat ? rec.mat->albedo_at(rec) : Vec3(1, 1, 1);

    // BSDF pdf for this light direction, to be used as MIS weight
    Ray to_light_ray(rec.point, to_light_dir);
    double bsdf_pdf = rec.mat->scattering_pdf(ray_in, rec, to_light_ray);
    double light_pdf = ls.pdf;
    // for delta lights (point/directional), bsdf_pdf = 0 so weight = 1 (only nee can sample), for area lights, both are nonzero so MIS balance
    double mis_weight = (bsdf_pdf > 0) ? power_heuristic(light_pdf, bsdf_pdf) : 1.0;

    direct = direct + ls.emission * attenuation * shadow_atten * cos_theta * mis_weight / light_pdf;
  }
  return direct;
}

// compute total light pdf at a point for a given direction -> used for MIS weight on the BSDF side
inline double total_light_pdf(const Vec3 &from, const Vec3 &dir, const LightList &lights) {
  double total = 0.0;
  for (const auto &light : lights) total += light->pdf(from, dir);
  return total;
}

inline Vec3 sky_color(const Ray &r) {
  if (g_sky_texture) {
    Vec3 unit = r.direction.normalize();
    double theta = acos(fmax(-1.0, fmin(1.0, unit.y)));
    double phi = atan2(unit.z, unit.x) + PI;
    double u = phi / (2.0 * PI);
    double v = theta / PI;
    return g_sky_texture->value(u, v, Vec3(0, 0, 0)) * g_sky_intensity;
  }
  Vec3 unit = r.direction.normalize();
  double t = 0.5 * (unit.y + 1.0);
  return Vec3(1, 1, 1) * (1.0 - t) + Vec3(0.5, 0.7, 1.0) * t;
}

inline Vec3 ray_color(const Ray &r, const Hittable &world, const LightList &lights, int depth, double prev_bsdf_pdf = -1.0, bool is_primary = true) {
  if (depth <= 0) return Vec3(0, 0, 0);
  HitRecord rec;

  bool hit = world.hit(r, 0.001, 1000, rec);
  double t_surface = hit ? rec.t : 1e30;

  // volumetric rays- computed first because transmittance attenuates the surface/sky term.
  Vec3 vol_contribution(0, 0, 0);
  double transmittance = 1.0;
  if (g_volume && is_primary) {
    double t0, t1;
    if (g_volume->bounds.intersect(r, t0, t1)) {
      t0 = fmax(t0, 0.001);
      t1 = fmin(t1, t_surface);
      if (t1 > t0) {
        transmittance = exp(-g_volume->sigma_t() * (t1 - t0));
        vol_contribution = integrate_volume_stochastic(r, t0, t1, *g_volume, world, lights);
      }
    }
  } else if (g_volume) {
    // secondary rays, just apply BeerLambert without scatter
    double t0, t1;
    if (g_volume->bounds.intersect(r, t0, t1)) {
      t0 = fmax(t0, 0.001);
      t1 = fmin(t1, t_surface);
      if (t1 > t0) transmittance = exp(-g_volume->sigma_t() * (t1 - t0));
    }
  }

  if (!hit) return vol_contribution + sky_color(r) * transmittance;

  Ray scattered;
  Vec3 attenuation;

  // emissive hit
  if (rec.mat->is_emissive()) {
    Vec3 Le = rec.front_face ? rec.mat->emit(rec.u, rec.v, rec.point) : Vec3(0, 0, 0);
    Vec3 surface;
    if (prev_bsdf_pdf < 0) surface = Le;
    else {
      double light_pdf = total_light_pdf(r.origin, r.direction, lights);
      double mis_weight = (light_pdf > 0) ? power_heuristic(prev_bsdf_pdf, light_pdf) : 1.0;
      surface = Le * mis_weight;
    }
    return vol_contribution + surface * transmittance;
  }

  // for non-emissive hitlist
  if (!rec.mat->scatter(r, rec, attenuation, scattered)) return vol_contribution;
  Vec3 result(0, 0, 0);

  // do NEE at every diffuse bounce
  if (!rec.mat->is_transmissive()) result = result + direct_light(r, rec, world, lights);
  double bsdf_pdf = rec.mat->scattering_pdf(r, rec, scattered);

  if (bsdf_pdf <= 0) {
    // specular — no pdf weighting needed
    result = result + ray_color(scattered, world, lights, depth - 1, -1.0) * attenuation;
  } else {
    // diffuse — explicit cos/pdf for correctness with any BSDF
    double cos_theta = fmax(0.0, rec.normal.dot(scattered.direction.normalize()));
    Vec3 indirect = ray_color(scattered, world, lights, depth - 1, bsdf_pdf);
    result = result + indirect * attenuation * (cos_theta / bsdf_pdf);
  }
  return vol_contribution + result * transmittance;
}

inline void render_rows(int start_row, int end_row, int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, const LightList &lights, std::vector<Vec3> &framebuffer) {
  for (int j = start_row; j < end_row; ++j) {
    for (int i = 0; i < width; ++i) {
      Vec3 pixel_color(0, 0, 0);
      for (int k = 0; k < samples_per_pixel; ++k) {
        double u = (i + random_double()) / (width - 1);
        double v = (j + random_double()) / (height - 1);
        Ray ray = camera.get_ray(u, v);
        pixel_color = pixel_color + ray_color(ray, world, lights, max_depth, -1.0);
      }
      // storing the frames
      framebuffer[j * width + i] = framebuffer[j * width + i] + pixel_color;
    }
  }
}

inline void render_tile(const Tile &tile, int img_width, int img_height, int max_depth, const Camera &camera, const Hittable &world, const LightList &lights, std::vector<Vec3> &framebuffer) {
  for (int j = tile.y; j < tile.y + tile.h; ++j) {
    for (int i = tile.x; i < tile.x + tile.w; ++i) {
      if (i >= img_width || j >= img_height) continue;
      double u = (i + random_double()) / (img_width - 1);
      double v = (j + random_double()) / (img_height - 1);
      Ray ray = camera.get_ray(u, v);
      framebuffer[j * img_width + i] = framebuffer[j * img_width + i] + ray_color(ray, world, lights, max_depth, -1.0);
    }
  }
}

inline void render_image(int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, const LightList &lights, std::vector<Vec3> &framebuffer) {
  constexpr int TILE_SIZE = 64;
  std::vector<Tile> tiles;
  for (int ty = 0; ty < height; ty += TILE_SIZE) {
    for (int tx = 0; tx < width; tx += TILE_SIZE) tiles.push_back({tx, ty, std::min(TILE_SIZE, width - tx), std::min(TILE_SIZE, height - ty)});
  }

  // atomic index into tile list — threads grab next tile when done (work stealing)
  int total_tiles = (int)tiles.size();

  int thread_count = std::thread::hardware_concurrency();
  std::vector<std::vector<Vec3>> thread_buffers(thread_count, std::vector<Vec3>(width * height, Vec3(0,0,0)));
  std::atomic<int> next_tile{0};
  std::vector<std::thread> threads;

  for (int t = 0; t < thread_count; ++t) {
    threads.emplace_back([&, t]() {
      while (true) {
        int index = next_tile.fetch_add(1);
        if (index >= total_tiles) break;
        for (int s = 0; s < samples_per_pixel; ++s) render_tile(tiles[index], width, height, max_depth, camera, world, lights, thread_buffers[t]);
      }
    });
  }
  for (auto &th : threads) th.join();

  // merge
  for (int i = 0; i < width * height; ++i)
  for (int t = 0; t < thread_count; ++t) framebuffer[i] = framebuffer[i] + thread_buffers[t][i];
}