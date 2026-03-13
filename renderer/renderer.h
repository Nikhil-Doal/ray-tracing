#pragma once
#include <memory>
#include <thread>
#include <vector>
#include "../core/camera.h"
#include "../core/hittable.h"
#include "../lights/light.h"

using LightList = std::vector<std::shared_ptr<Light>>;

inline Vec3 direct_light(const HitRecord &rec, const Hittable &world, const LightList &lights) {
  Vec3 direct(0,0,0);
  for (const auto &light : lights) {
    LightSample ls = light->sample(rec.point);

    Vec3 to_light = ls.point - rec.point;
    double dist = to_light.norm();
    Vec3 to_light_dir = to_light / dist;

    // check if surface faces the light
    double cos_theta = rec.normal.dot(to_light_dir);
    if (cos_theta <= 0) continue;

    // shadow ray 
    HitRecord shadow_rec;
    Ray shadow_ray(rec.point + rec.normal * 1e-4, to_light_dir);
    if (world.hit(shadow_ray, 0.001, dist - 0.001, shadow_rec)) continue; // occluded

    // Lambert BRDF * light emission * cos falloff / pdf
    direct = direct + ls.emission * cos_theta / ls.pdf;
  }
  return direct;
}

inline Vec3 ray_color(const Ray &r, const Hittable &world, const LightList &lights, int depth) {
  if (depth <= 0) return Vec3(0,0,0);
  HitRecord rec;

  if (world.hit(r, 0.001, 1000, rec)) {
    Ray scattered;
    Vec3 attenuation;
    Vec3 direct = direct_light(rec, world, lights);

    if (rec.mat->scatter(r, rec, attenuation, scattered)) {
      Vec3 indirect = ray_color(scattered, world, lights, depth-1) * attenuation;
      return direct + indirect;
    }

    return direct * attenuation;
  }
  // sky gradient
  Vec3 unit = r.direction.normalize();
  double t = 0.5*(unit.y + 1.0);
  return Vec3(1,1,1)*(1.0-t) + Vec3(0.5,0.7,1.0)*t; 
}

inline void render_rows(int start_row, int end_row, int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, const LightList &lights, std::vector<Vec3> &framebuffer) {
  for (int j = start_row; j < end_row; ++j) {
    for (int i = 0; i < width; ++i) {
      Vec3 pixel_color(0,0,0);
      for (int k = 0; k < samples_per_pixel; ++k) {
        
        double u = (i + random_double()) / (width - 1);
        double v = (j + random_double()) / (height - 1);

        Ray ray = camera.get_ray(u, v);
        pixel_color = pixel_color + ray_color(ray, world, lights, max_depth);
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