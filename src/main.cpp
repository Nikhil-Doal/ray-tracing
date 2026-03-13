#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <fstream>
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../objects/sphere.h"
#include "../objects/plane.h"
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
#include "../textures/image_texture.h"
#include <memory>
 

int main() {
  int width = 1080;
  int height = 720;
  const int samples_per_pixel = 100;
  const int max_depth = 50;

  // Making the scene
  HittableList world;

  auto black = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.8, 0.8, 0.8)));
  auto tinted_glass = std::make_shared<Dielectric>(1.5, std::make_shared<SolidColor>(Vec3(0,0,0.3)));
  auto green = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0, 1, 0) * 0.2));
  auto gold_tint = std::make_shared<Metal>(std::make_shared<SolidColor>(Vec3(0.8, 0.8, 0.8)), 0.1);
  auto earth_mat = std::make_shared<Lambertian>(std::make_shared<ImageTexture>("../assets/earth.jpg"));

  world.add(std::make_shared<Plane>(Vec3(0, -10 , 0), Vec3(0,1,0), gold_tint));
  
  // std::vector<Triangle*> triangles = load_obj_triangle("assets/models/bunny.obj", earth_mat, 80.0, Vec3(0,0,0));
  // for (auto triangle : triangles) world.add(triangle);
  world.add(std::make_shared<Sphere>(Vec3(0, 0, 0), 20, earth_mat));
  // world.add(std::make_shared<Sphere>(Vec3(4, 0, 0), 2, tinted_glass));
  

  BVHNode world_bvh(world.objects, 0, world.objects.size());
  
  // Camera setup
  double aspect_ratio = double(width)/height;
  Vec3 lookfrom(-20, 20, -20);
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
    
    save_png("../image.png", width, height, framebuffer, i);
    // if (i % 10 == 0) {
    //   std::cout << "Saved sample" << std::endl;
    // }
  }
}