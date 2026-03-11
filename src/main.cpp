#define TINYOBJLOADER_IMPLEMENTATION
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <fstream>
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../objects/sphere.h"
#include "../objects/hittable_list.h"
#include "../materials/material.h"
#include "../materials/lambertian.h"
#include "../materials/metal.h"
#include "../materials/dielectric.h"
#include "../core/camera.h"
#include "../utils/random_scene.h"
#include "../objects/bvh_node.h"
#include "../objects/obj_loader.h"
#include "../renderer/renderer.h"

int main() {
  int width = 400;
  int height = 200;
  const int samples_per_pixel = 100;
  const int max_depth = 50;
  srand(time(0));

  // Making the scene
  HittableList world; // = random_scene();
  Material *white = new Lambertian(Vec3(0.8,0.8,0.8));
  std::vector<Triangle*> triangles = load_obj_triangle("assets/models/bunny.obj", white, 10.0, Vec3(0,0,0));
  for(auto t : triangles)
    world.add(t);
  BVHNode world_bvh(world.objects, 0, world.objects.size());
  
  // Camera setup
  double aspect_ratio = double(width)/height;
  Vec3 lookfrom(3, 4, 1);
  Vec3 lookat(0, 0, 0);
  Vec3 vup(0, 1, 0);
  double focus_dist = (lookfrom - lookat).norm();
  double aperture = 0.0;

  Camera camera(lookfrom, lookat, vup, 90, aspect_ratio, aperture, focus_dist);

  // Storing pixels in frame buffer
  std::vector<Vec3> framebuffer(width * height);

  // Rendering pixels
  render_image(width, height, samples_per_pixel, max_depth, camera, world_bvh, framebuffer);

  std::ofstream ofs("image.ppm", std::ios::binary);
  ofs << "P6\n" << width << " " << height << "\n255\n";

  for (int j = height - 1; j >= 0; --j) {
      for (int i = 0; i < width; ++i) {
          Vec3 pixel_color = framebuffer[j * width + i];
          pixel_color = pixel_color / samples_per_pixel;
          pixel_color = Vec3(sqrt(pixel_color.x), sqrt(pixel_color.y), sqrt(pixel_color.z));

          auto clamp = [](double x, double min, double max) {
              if (x < min) return min;
              if (x > max) return max;
              return x;
          };

          unsigned char color[3] = {
              static_cast<unsigned char>(256 * clamp(pixel_color.x, 0.0, 0.999)),
              static_cast<unsigned char>(256 * clamp(pixel_color.y, 0.0, 0.999)),
              static_cast<unsigned char>(256 * clamp(pixel_color.z, 0.0, 0.999))
          };
          ofs.write(reinterpret_cast<char*>(color), 3);
      }
  }

  ofs.close();
}