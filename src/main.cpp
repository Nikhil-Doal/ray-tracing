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
// #include "../utils/random_scene.h"
#include "../objects/bvh_node.h"
#include "../objects/obj_loader.h"
#include "../renderer/renderer.h"
#include "../utils/image_writer.h"
#include "../textures/solid_color.h"
 

int main() {
  int width = 400;
  int height = 200;
  const int samples_per_pixel = 100;
  const int max_depth = 50;
  srand(time(0));

  // Making the scene
  HittableList world;
  Material *black = new Lambertian(new SolidColor(Vec3(0, 0, 0)));
  Material *tinted_glass = new Dielectric(1.5, new SolidColor(Vec3(0,0,0.3)));
  Material *green = new Lambertian(new SolidColor(Vec3(0, 1, 0) * 0.2));
  Material *gold_tint = new Metal(new SolidColor(Vec3(1, 0.84, 0)), 0.1);


  world.add(new Sphere(Vec3(0, 0, 0), 2, gold_tint));
  world.add(new Sphere(Vec3(4, 0, 0), 2, tinted_glass));
  
  world.add(new Sphere(Vec3(0, -1001, 0), 1000, green));

  BVHNode world_bvh(world.objects, 0, world.objects.size());
  
  // Camera setup
  double aspect_ratio = double(width)/height;
  Vec3 lookfrom(-3, 3, 3);
  Vec3 lookat(0, 0, 0);
  Vec3 vup(0, 1, 0);
  double focus_dist = (lookfrom - lookat).norm();
  double aperture = 0.0;

  Camera camera(lookfrom, lookat, vup, 90, aspect_ratio, aperture, focus_dist);

  // Storing pixels in frame buffer
  std::vector<Vec3> framebuffer(width * height);

  for (int i = 0; i < samples_per_pixel; ++i) {
    // Rendering pixels
    render_image(width, height, 1, max_depth, camera, world_bvh, framebuffer);
    
    save_png("image.png", width, height, framebuffer, i);
    // if (i % 10 == 0) {
    //   std::cout << "Saved sample" << std::endl;
    // }
  }
}