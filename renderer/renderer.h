#pragma once
#include <memory>
#include <thread>
#include <vector>
#include "../core/camera.h"
#include "../core/hittable.h"
#include "../lights/light.h"

using LightList = std::vector<std::shared_ptr<Light>>;

inline double power_heuristic(double pdf_a, double pdf_b) {
    double a2 = pdf_a * pdf_a;
    double b2 = pdf_b * pdf_b;
    return (a2 + b2) < 1e-6 ? 0.0 : a2 / (a2 + b2);
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
    Vec3 attenuation(1,1,1);
    Ray scattered_dummy;
    if (rec.mat) rec.mat->scatter(ray_in, rec, attenuation, scattered_dummy);

    // BSDF pdf for this light direction, to be used as MIS weight
    Ray to_light_ray(rec.point, to_light_dir);
    double bsdf_pdf = rec.mat->scattering_pdf(ray_in, rec, to_light_ray);
    double light_pdf = ls.pdf;

    // for delta lights (point/directional), bsdf_pdf = 0 so weight = 1 (only nee can sample)
    // for area lights, both are nonzero so MIS balance
    double mis_weight = (bsdf_pdf > 0) ? power_heuristic(light_pdf, bsdf_pdf) : 1.0;
    direct = direct + ls.emission * attenuation * cos_theta * mis_weight / light_pdf;
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
      Vec3 Le = rec.mat->emit(rec.u, rec.v, rec.point);

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
      result = result + ray_color(scattered, world, lights, depth - 1, -1.0) * attenuation;
    } else {
      // diffuse — pass bsdf_pdf for MIS weighting downstream
      double cos_theta = rec.normal.dot(scattered.direction.normalize());
      if (cos_theta > 0) {
        Vec3 indirect = ray_color(scattered, world, lights, depth - 1, bsdf_pdf) * attenuation;
        // cos_theta / bsdf_pdf = 1 for Lambertian but kept explicit for other BSDFs
        result = result + indirect * (cos_theta / bsdf_pdf);
      }
    }
    return result;
  }
  // sky
  Vec3 unit = r.direction.normalize();
  double t = 0.5*(unit.y + 1.0);
  Vec3 sky = Vec3(1,1,1)*(1.0-t) + Vec3(0.5,0.7,1.0)*t;
  return (prev_bsdf_pdf < 0) ? sky : sky * 0.3;
}

inline void render_rows(int start_row, int end_row, int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, const LightList &lights, std::vector<Vec3> &framebuffer) {
  for (int j = start_row; j < end_row; ++j) {
    for (int i = 0; i < width; ++i) {
      Vec3 pixel_color(0,0,0);
      for (int k = 0; k < samples_per_pixel; ++k) {
        double u = (i + random_double()) / (width - 1);
        double v = (j + random_double()) / (height - 1);
        Ray ray = camera.get_ray(u, v);
        pixel_color = pixel_color + ray_color(ray, world, lights, max_depth, -1.0);
      }
      // storing the frames
      framebuffer[j*width + i] = framebuffer[j*width + i] + pixel_color;
    }
  }
}

inline void render_image(int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, const LightList &lights, std::vector<Vec3> &framebuffer) {
  int thread_count = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;

  int rows_per_thread = height / thread_count;
  for (int t = 0; t < thread_count; ++t) {
    int start = t * rows_per_thread;
    int end = (t == thread_count - 1) ? height : start + rows_per_thread;
    threads.emplace_back(render_rows, start, end, width, height, samples_per_pixel, max_depth, std::ref(camera), std::ref(world), std::ref(lights), std::ref(framebuffer));
  }
  
  for (auto &t :threads) {
    t.join();
  }
}
