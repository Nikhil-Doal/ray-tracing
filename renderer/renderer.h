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
#include "../materials/material.h"


inline std::shared_ptr<Texture> g_sky_texture = nullptr;
inline float g_sky_intensity = 1.0f;

using LightList = std::vector<std::shared_ptr<Light>>;
struct Tile {
  int x, y, w, h;
};

// Max throughput magnitude before clamping — kills fireflies from bad pdf ratios
constexpr double MAX_THROUGHPUT = 15.0;

inline double power_heuristic(double pdf_a, double pdf_b) {
  double a2 = pdf_a * pdf_a;
  double b2 = pdf_b * pdf_b;
  return (a2 + b2) < 1e-6 ? 0.0 : a2 / (a2 + b2);
}

inline Vec3 sample_sky(const Vec3 &dir) {
  if (g_sky_texture) {
    Vec3 unit = dir.normalize();
    double theta = acos(fmax(-1.0, fmin(1.0, unit.y)));
    double phi = atan2(unit.z, unit.x) + PI;
    double u = phi / (2.0 * PI);
    double v = theta / PI;
    return g_sky_texture->value(u, v, Vec3(0,0,0)) * g_sky_intensity;
  }
  Vec3 unit = dir.normalize();
  double t = 0.5 * (unit.y + 1.0);
  return Vec3(1,1,1) * (1.0 - t) + Vec3(0.5, 0.7, 1.0) * t;
}

inline Vec3 clamp_vec(const Vec3 &v, double max_component) {
  return Vec3(
    std::isfinite(v.x) ? fmin(fmax(v.x, 0.0), max_component) : 0.0,
    std::isfinite(v.y) ? fmin(fmax(v.y, 0.0), max_component) : 0.0,
    std::isfinite(v.z) ? fmin(fmax(v.z, 0.0), max_component) : 0.0
  );
}

inline Vec3 direct_light(const Ray &ray_in, const HitRecord &rec, const Hittable &world, const LightList &lights) {
  Vec3 direct(0,0,0);
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
    HitRecord shadow_rec;
    Ray shadow_ray(rec.point + rec.normal * 1e-4, to_light_dir);
    if (world.hit(shadow_ray, 0.001, dist - 0.01, shadow_rec)) {
      if (!shadow_rec.mat->is_emissive()) continue;
    }

    // get surface attenuation
    Vec3 attenuation = rec.mat ? rec.mat->albedo_at(rec) : Vec3(1,1,1);

    // BSDF pdf for this light direction, to be used as MIS weight
    Ray to_light_ray(rec.point, to_light_dir);
    double bsdf_pdf = rec.mat->scattering_pdf(ray_in, rec, to_light_ray);
    double light_pdf = ls.pdf;

    // for delta lights (point/directional), bsdf_pdf = 0 so weight = 1 (only nee can sample)
    // for area lights, both are nonzero so MIS balance
    double mis_weight = (bsdf_pdf > 0) ? power_heuristic(light_pdf, bsdf_pdf) : 1.0;

    Vec3 contribution = ls.emission * attenuation * (1.0 / PI) * cos_theta * mis_weight / light_pdf;
    direct = direct + clamp_vec(contribution, MAX_THROUGHPUT);
  }
  return direct;
}

// compute total light pdf at a point for a given direction -> used for MIS weight on the BSDF side
inline double total_light_pdf(const Vec3 &from, const Vec3 &dir, const LightList &lights) {
  double total = 0.0;
  for (const auto &light : lights)
    total += light->pdf(from, dir);
  return total;
}

inline Vec3 ray_color(const Ray &r, const Hittable &world, const LightList &lights, int depth, double prev_bsdf_pdf = -1.0) {
  if (depth <= 0) return Vec3(0,0,0);

  HitRecord rec;

  if (world.hit(r, 0.001, 1000, rec)) {
    Ray scattered;
    Vec3 attenuation;

    // emissive hit
    if (rec.mat->is_emissive()) {
      Vec3 Le = rec.front_face ? rec.mat->emit(rec.u, rec.v, rec.point) : Vec3(0,0,0);

      if (prev_bsdf_pdf < 0) return Le;

      // diffuse bounce - check against NEE to prevent double counting
      double light_pdf = total_light_pdf(r.origin, r.direction, lights);
      double mis_weight = (light_pdf > 0) ? power_heuristic(prev_bsdf_pdf, light_pdf) : 1.0;
      return Le * mis_weight;
    }

    // for non-emissive hitlist
    if (!rec.mat->scatter(r, rec, attenuation, scattered)) return Vec3(0,0,0);
    Vec3 result(0,0,0);

    // do NEE at every diffuse bounce
    if (!rec.mat->is_transmissive()) result = result + direct_light(r, rec, world, lights);
    double bsdf_pdf = rec.mat->scattering_pdf(r, rec, scattered);

    if (bsdf_pdf <= 0) {
      // specular — no pdf weighting needed
      result = result + ray_color(scattered, world, lights, depth - 1, -1.0) * attenuation;
    } 
    else {
    // Diffuse bounce — cosine-weighted sampling already accounts for BRDF/PDF

    Vec3 indirect = ray_color(scattered, world, lights, depth - 1, bsdf_pdf);
    result = result + indirect * attenuation;
  }
    return result;
  }
  // sky
  return sample_sky(r.direction);
}

inline void render_rows(int start_row, int end_row, int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, const LightList &lights, std::vector<Vec3> &framebuffer) {
  for (int j = start_row; j < end_row; ++j) {
    for (int i = 0; i < width; ++i) {
      Vec3 pixel_color(0,0,0);
      for (int k = 0; k < samples_per_pixel; ++k) {
        double u = (i + random_double()) / (width - 1);
        double v = (j + random_double()) / (height - 1);
        Ray ray = camera.get_ray(u, v);
        Vec3 sample = ray_color(ray, world, lights, max_depth, -1.0);
        sample = clamp_vec(sample, MAX_THROUGHPUT);
        pixel_color = pixel_color + sample;
      }
      // storing the frames
      framebuffer[j*width + i] = framebuffer[j*width + i] + pixel_color;
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
      Vec3 sample = ray_color(ray, world, lights, max_depth, -1.0);
      sample = clamp_vec(sample, MAX_THROUGHPUT);
      framebuffer[j*img_width + i] = framebuffer[j*img_width + i] + sample;
    }
  }
}

inline void render_image(int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, const LightList &lights, std::vector<Vec3> &framebuffer) {
  constexpr int TILE_SIZE = 64;
  std::vector<Tile> tiles;

  for (int ty = 0; ty < height; ty += TILE_SIZE) {
    for (int tx = 0; tx < width; tx += TILE_SIZE) tiles.push_back({tx, ty, std::min(TILE_SIZE, width - tx), std::min(TILE_SIZE, height - ty)});
  }

  // atomic index into tile list — threads grab next tile when done
  std::atomic<int> next_tile{0};
  int total_tiles = (int)tiles.size();
  
  int thread_count = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;

  for (int t = 0; t < thread_count; ++t) {
    threads.emplace_back([&]() {
      while (true)
      {
        int index = next_tile.fetch_add(1);
        if (index >= total_tiles) break;

        for(int s = 0; s < samples_per_pixel; ++s) render_tile(tiles[index], width, height, max_depth, camera, world, lights, framebuffer);
      }
    });  
  }
  for (auto &t :threads) t.join();
}