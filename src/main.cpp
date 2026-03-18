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
#include "../utils/random_scene.h"
#include "../objects/bvh_node.h"
#include "../objects/obj_loader.h"
#include "../renderer/renderer.h"
#include "../utils/image_writer.h"
#include "../textures/solid_color.h"
#include "../textures/image_texture.h"
#include "../lights/point_light.h"
#include "../lights/directional_light.h"
#include "../objects/rotate.h"
#include "../materials/diffuse_light.h"
#include "../lights/area_light.h"
#include <memory>

// generate a synthetic normal map PNG with a bump pattern
// this creates a sine-wave bump so the effect is obvious
void generate_test_normal_map(const std::string &path, int size = 256) {
  std::vector<unsigned char> pixels(size * size * 3);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      // create a sine wave bump pattern
      double freq = 8.0;
      double dx = cos(freq * 2.0 * 3.14159 * x / size) * 0.5;
      double dy = cos(freq * 2.0 * 3.14159 * y / size) * 0.5;
      double dz = sqrt(fmax(0.0, 1.0 - dx*dx - dy*dy));

      // encode [-1,1] -> [0,255]
      int idx = (y * size + x) * 3;
      pixels[idx + 0] = (unsigned char)((dx * 0.5 + 0.5) * 255);
      pixels[idx + 1] = (unsigned char)((dy * 0.5 + 0.5) * 255);
      pixels[idx + 2] = (unsigned char)((dz * 0.5 + 0.5) * 255);
    }
  }
  stbi_write_png(path.c_str(), size, size, 3, pixels.data(), size * 3);
  std::cout << "Generated test normal map: " << path << "\n";
}

int main() {
  int width = 1080;
  int height = 720;
  const int samples_per_pixel = 64;
  const int max_depth = 20;

  HittableList world;
  LightList lights;

  // generate a synthetic normal map so we know the data is correct
  generate_test_normal_map("../../assets/test_normal.png", 256);

  // LEFT SIDE: normal-mapped floor
  auto nmap_mat = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.9, 0.9, 0.9)));
  nmap_mat->set_normal_map(std::make_shared<ImageTexture>("../../assets/test_normal.png", true));

  // two triangles for the left half of the floor
  // UVs tile 3x over the surface so the bump pattern repeats visibly
  world.add(std::make_shared<Triangle>(
    Vec3(-30, 0, -30), Vec3(0, 0, 30), Vec3(0, 0, -30), nmap_mat,
    Vec3(0,0,0), Vec3(3,3,0), Vec3(3,0,0),
    Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
  world.add(std::make_shared<Triangle>(
    Vec3(-30, 0, -30), Vec3(-30, 0, 30), Vec3(0, 0, 30), nmap_mat,
    Vec3(0,0,0), Vec3(0,3,0), Vec3(3,3,0),
    Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));

  // RIGHT SIDE: plain flat floor (no normal map) for comparison
  auto flat_mat = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.9, 0.9, 0.9)));

  world.add(std::make_shared<Triangle>(
    Vec3(0, 0, -30), Vec3(30, 0, 30), Vec3(30, 0, -30), flat_mat,
    Vec3(0,0,0), Vec3(1,1,0), Vec3(1,0,0),
    Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
  world.add(std::make_shared<Triangle>(
    Vec3(0, 0, -30), Vec3(0, 0, 30), Vec3(30, 0, 30), flat_mat,
    Vec3(0,0,0), Vec3(0,1,0), Vec3(1,1,0),
    Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));

  // strong directional light from the side to show bumps clearly
  lights.push_back(std::make_shared<DirectionalLight>(Vec3(1, 1, 0.5), Vec3(1,1,1), 1.5));

  // area light above
  // auto light_mat = std::make_shared<DiffuseLight>(Vec3(1.0, 0.95, 0.8) * 8.0);
  // world.add(std::make_shared<Sphere>(Vec3(0, 20, 0), 3.0, light_mat));
  // lights.push_back(std::make_shared<SphereAreaLight>(Vec3(0, 20, 0), 3.0, Vec3(1.0, 0.95, 0.8), 8.0));

  BVHNode world_bvh(world.objects, 0, world.objects.size());

  // camera looking down at the floor at an angle
  double aspect_ratio = double(width) / height;
  Vec3 lookfrom(0, 15, 25);
  Vec3 lookat(0, 0, 0);
  Camera camera(lookfrom, lookat, Vec3(0,1,0), 80, aspect_ratio, 0.0, (lookfrom - lookat).norm());

  std::vector<Vec3> framebuffer(width * height);
  render_image(width, height, samples_per_pixel, max_depth, camera, world_bvh, lights, framebuffer);
  save_png("../../image.png", width, height, framebuffer, samples_per_pixel);

  std::cout << "Done! Left half = normal mapped, Right half = flat. Compare them.\n";
}