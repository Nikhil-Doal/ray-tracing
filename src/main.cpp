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
#include "../lights/point_light.h"
#include "../lights/directional_light.h"
#include "../objects/transform.h"
#include <memory>
 

int main() {
  int width = 1080;
  int height = 720;
  const int samples_per_pixel = 10;
  const int max_depth = 50;

  // Making the scene
  HittableList world;
  LightList lights;

  // lights.push_back(std::make_shared<PointLight>(Vec3(-20, 20, 20), Vec3(1,1,1), 100.0));
  lights.push_back(std::make_shared<DirectionalLight>(Vec3(-1,1,1), Vec3(1,1,1), 1));

  auto black = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(1, 0, 0)));
  auto tinted_glass = std::make_shared<Dielectric>(1.5, std::make_shared<SolidColor>(Vec3(1,1,1)));
  auto green = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0, 1, 0) * 0.2));
  auto gold_tint = std::make_shared<Metal>(std::make_shared<SolidColor>(Vec3(0.8, 0.8, 0.8)), 0.0);
  auto earth_mat = std::make_shared<Lambertian>(std::make_shared<ImageTexture>("../assets/earth.png"));

  world.add(std::make_shared<Plane>(Vec3(0, 0 , 0), Vec3(0,1,0), gold_tint));
  
  // load triangles into a hittable list first
  auto bunny_list = std::make_shared<HittableList>();
  auto triangles = load_obj_triangle("../assets/models/bunny.obj", black, 200.0, Vec3(0,0,0));
  for (auto t : triangles)
      bunny_list->add(t);

  // build a BVH over just the bunny
  auto bunny_bvh = std::make_shared<BVHNode>(bunny_list->objects, 0, bunny_list->objects.size());

  // rotate the whole thing as one unit
  auto rotated_bunny = std::make_shared<Transform>(bunny_bvh, -90);
  world.add(rotated_bunny);

  // world.add(std::make_shared<Sphere>(Vec3(0, 0, 0), 20, earth_mat));
  // world.add(std::make_shared<Sphere>(Vec3(4, 0, 0), 2, tinted_glass));
  

  BVHNode world_bvh(world.objects, 0, world.objects.size());
  
  // Camera setup
  double aspect_ratio = double(width)/height;
  Vec3 lookfrom(30, 50, 30);
  Vec3 lookat(0, 5, 0);
  Vec3 vup(0, 1, 0);
  double focus_dist = (lookfrom - lookat).norm();
  double aperture = 0.0;

  Camera camera(lookfrom, lookat, vup, 90, aspect_ratio, aperture, focus_dist);

  // Storing pixels in frame buffer
  std::vector<Vec3> framebuffer(width * height);

  for (int i = 0; i < samples_per_pixel; ++i) {
    // Rendering pixels
    render_image(width, height, 1, max_depth, camera, world_bvh, lights, framebuffer);
    
    save_png("../image.png", width, height, framebuffer, i);
    // if (i % 10 == 0) {
    //   std::cout << "Saved sample" << std::endl;
    // }
  }
}