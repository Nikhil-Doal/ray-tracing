#define TINYOBJLOADER_IMPLEMENTATION
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
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
  Mesh *bunny = load_obj("assets/models/bunny.obj", white, 10, Vec3(0, 0, 0));
  world.add(bunny);
  BVHNode world_bvh(world.objects, 0, world.objects.size());

  std::cout << "P3\n" << width << " " << height << "\n255\n";
  
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

  for (int j = height - 1; j >= 0; --j) {
    for (int i = 0; i < width; ++i) {

      Vec3 pixel_color = framebuffer[j * width + i];
      pixel_color = pixel_color / samples_per_pixel;

      pixel_color = Vec3(
          sqrt(pixel_color.x),
          sqrt(pixel_color.y),
          sqrt(pixel_color.z)
      );

      auto clamp = [](double x, double min, double max) {
        if (x < min) return min;
        if (x > max) return max;
        return x;
      };

      int ir = int(256 * clamp(pixel_color.x, 0.0, 0.999));
      int ig = int(256 * clamp(pixel_color.y, 0.0, 0.999));
      int ib = int(256 * clamp(pixel_color.z, 0.0, 0.999));

      std::cout << ir << " " << ig << " " << ib << "\n";
    }
  }
}


