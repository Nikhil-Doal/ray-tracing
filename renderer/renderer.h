#pragma once

#include <thread>
#include <vector>
#include "../core/camera.h"
#include "../core/hittable.h"

Vec3 ray_color(const Ray &r, const Hittable &world, int depth) {
  if (depth <= 0) return Vec3(0,0,0);
  HitRecord rec;

  if (world.hit(r, 0.001, 1000, rec)) {
    Ray scattered;
    Vec3 attenuation;

    if (rec.mat->scatter(r, rec, attenuation, scattered)) {
      return ray_color(scattered, world, depth-1) * attenuation;
    }

    return Vec3(0,0,0);
  }


  Vec3 unit = r.direction.normalize();
  double t = 0.5*(unit.y + 1.0);

  return Vec3(1,1,1)*(1.0-t) + Vec3(0.5,0.7,1.0)*t; // sky gradient
}

void render_rows(int start_row, int end_row, int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, std::vector<Vec3> &framebuffer) {
  for (int j = start_row; j < end_row; ++j) {
    for (int i = 0; i < width; ++i) {
      Vec3 pixel_color(0,0,0);
      for (int k = 0; k < samples_per_pixel; ++k) {
        
        double u = (i + random_double()) / (width - 1);
        double v = (j + random_double()) / (height - 1);

        Ray ray = camera.get_ray(u, v);
        pixel_color = pixel_color + ray_color(ray, world, max_depth);
      }
      // storing the frames
      framebuffer[j*width + i] = pixel_color;
    }
  }
}

inline void render_image(int width, int height, int samples_per_pixel, int max_depth, const Camera &camera, const Hittable &world, std::vector<Vec3> &framebuffer) {
  int thread_count = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;

  int rows_per_thread = height / thread_count;
  for (int t = 0; t < thread_count; ++t) {
    int start = t * rows_per_thread;
    int end = (t == thread_count - 1) ? height : start + rows_per_thread;
    threads.emplace_back(render_rows, start, end, width, height, samples_per_pixel, max_depth, std::ref(camera), std::ref(world), std::ref(framebuffer));
  }
  
  for (auto &t :threads) {
    t.join();
  }
}