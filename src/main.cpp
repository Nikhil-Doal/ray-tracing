#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

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
#include "../utils/image_writer.h"

int main() {
  int width = 1920;
  int height = 1080;
  const int samples_per_pixel = 1000;
  const int max_depth = 100;
  srand(time(0));

  // Making the scene
  HittableList world = random_scene();
  // Material *white = new Lambertian(Vec3(0.8,0.8,0.8));
  // std::vector<Triangle*> triangles = load_obj_triangle("assets/models/bunny.obj", white, 10.0, Vec3(0,0,0));
  // for(auto t : triangles)
  //   world.add(t);
  BVHNode world_bvh(world.objects, 0, world.objects.size());
  
  // Camera setup
  double aspect_ratio = double(width)/height;
  Vec3 lookfrom(2.5, 2, 1);
  Vec3 lookat(2, 1, 0);
  Vec3 vup(0, 1, 0);
  double focus_dist = (lookfrom - lookat).norm();
  double aperture = 0.0;

  Camera camera(lookfrom, lookat, vup, 90, aspect_ratio, aperture, focus_dist);

  // Storing pixels in frame buffer
  std::vector<Vec3> framebuffer(width * height);

  // Rendering pixels
  render_image(width, height, samples_per_pixel, max_depth, camera, world_bvh, framebuffer);
  save_png("image.png", width, height, framebuffer, samples_per_pixel);
}