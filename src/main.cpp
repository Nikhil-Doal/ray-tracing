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
#include "../materials/glossy.h"
#include "../core/camera.h"
#include "../utils/random_scene.h"
#include "../objects/bvh_node.h"
#include "../objects/obj_loader.h"
#include "../renderer/renderer.h"
#include "../utils/image_writer.h"
#include "../textures/solid_color.h"
#include "../textures/image_texture.h"
#include "../textures/hdr_texture.h"
#include "../lights/point_light.h"
#include "../lights/directional_light.h"
#include "../objects/rotate.h"
#include "../materials/diffuse_light.h"
#include "../lights/area_light.h"
#include <memory>

int main() {
  int width = 1080;
  int height = 720;
  const int samples_per_pixel = 128;
  const int max_depth = 20;

  HittableList world;
  LightList lights;

  // HDR sky
  g_sky_texture = std::make_shared<HdrTexture>("../../assets/sky.hdr");
  g_sky_intensity = 1.0f;

  float tile = 1.0f;

  // ---- LEFT SIDE: Glossy + normal map + bump map ----
  auto nmap_mat = std::make_shared<Glossy>(std::make_shared<ImageTexture>("../../assets/bricks.jpg"), 0.3, 0.5);
  nmap_mat->set_normal_map(std::make_shared<ImageTexture>("../../assets/bricks_normal.jpg", true), 5.0);
  nmap_mat->set_bump_map(std::make_shared<ImageTexture>("../../assets/bricks_bump.jpg", true), 10.0);

  world.add(std::make_shared<Sphere>(Vec3(0,0,0), 10, nmap_mat));
  // Left floor
  world.add(std::make_shared<Triangle>(
    Vec3(-15, 0, -15), Vec3(0, 0, 15), Vec3(0, 0, -15), nmap_mat,
    Vec3(0,0,0), Vec3(tile,tile,0), Vec3(tile,0,0),
    Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
  world.add(std::make_shared<Triangle>(
    Vec3(-15, 0, -15), Vec3(-15, 0, 15), Vec3(0, 0, 15), nmap_mat,
    Vec3(0,0,0), Vec3(0,tile,0), Vec3(tile,tile,0),
    Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));

  // Left wall
  world.add(std::make_shared<Triangle>(
    Vec3(-15, 0, -5), Vec3(0, 0, -5), Vec3(0, 8, -5), nmap_mat,
    Vec3(0,0,0), Vec3(tile,0,0), Vec3(tile,tile*0.5f,0),
    Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));
  world.add(std::make_shared<Triangle>(
    Vec3(-15, 0, -5), Vec3(0, 8, -5), Vec3(-15, 8, -5), nmap_mat,
    Vec3(0,0,0), Vec3(tile,tile*0.5f,0), Vec3(0,tile*0.5f,0),
    Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));

  // ---- RIGHT SIDE: Glossy flat (no normal/bump map) ----
  auto flat_mat = std::make_shared<Glossy>(std::make_shared<ImageTexture>("../../assets/bricks.jpg"), 0.3, 0.5);

  // Right floor
  world.add(std::make_shared<Triangle>(
    Vec3(0, 0, -15), Vec3(15, 0, 15), Vec3(15, 0, -15), flat_mat,
    Vec3(0,0,0), Vec3(tile,tile,0), Vec3(tile,0,0),
    Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
  world.add(std::make_shared<Triangle>(
    Vec3(0, 0, -15), Vec3(0, 0, 15), Vec3(15, 0, 15), flat_mat,
    Vec3(0,0,0), Vec3(0,tile,0), Vec3(tile,tile,0),
    Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));

  // Right wall
  world.add(std::make_shared<Triangle>(
    Vec3(0, 0, -5), Vec3(15, 0, -5), Vec3(15, 8, -5), flat_mat,
    Vec3(0,0,0), Vec3(tile,0,0), Vec3(tile,tile*0.5f,0),
    Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));
  world.add(std::make_shared<Triangle>(
    Vec3(0, 0, -5), Vec3(15, 8, -5), Vec3(0, 8, -5), flat_mat,
    Vec3(0,0,0), Vec3(tile,tile*0.5f,0), Vec3(0,tile*0.5f,0),
    Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));

  // Area light
  auto light_mat = std::make_shared<DiffuseLight>(Vec3(1.0, 0.95, 0.8) * 8.0);
  world.add(std::make_shared<Sphere>(Vec3(0, 20, 0), 3.0, light_mat));
  lights.push_back(std::make_shared<SphereAreaLight>(Vec3(0, 20, 0), 3.0, Vec3(1.0, 0.95, 0.8), 8.0));

  BVHNode world_bvh(world.objects, 0, world.objects.size());

  double aspect_ratio = double(width) / height;
  Vec3 lookfrom(0, 6, 14);
  Vec3 lookat(0, 2, -2);
  Camera camera(lookfrom, lookat, Vec3(0,1,0), 65, aspect_ratio, 0.0, (lookfrom - lookat).norm());

  std::vector<Vec3> framebuffer(width * height);
  render_image(width, height, samples_per_pixel, max_depth, camera, world_bvh, lights, framebuffer);
  save_png("../../image.png", width, height, framebuffer, samples_per_pixel);

  std::cout << "Done! Left = Glossy + normal/bump map, Right = Glossy flat.\n";
  std::cout << "HDR sky enabled.\n";
}