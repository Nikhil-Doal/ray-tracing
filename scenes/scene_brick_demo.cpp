#include "scene_registry.h"
#include "../objects/sphere.h"
#include "../objects/triangle.h"
#include "../materials/glossy.h"
#include "../materials/diffuse_light.h"
#include "../lights/area_light.h"
#include "../textures/image_texture.h"
#include "../textures/solid_color.h"

static SceneDesc build_brick_demo() {
  SceneDesc scene;

  scene.width  = 1080;
  scene.height = 720;
  scene.samples_per_pixel = 128;
  scene.max_depth = 20;

  scene.lookfrom = Vec3(0, 6, 14);
  scene.lookat   = Vec3(0, 2, -2);
  scene.vfov     = 65.0;
  scene.aperture = 0.0;

  scene.sky_hdr_path  = "../../assets/sky.hdr";
  scene.sky_intensity = 1.0f;

  float tile = 1.0f;

  // ---- LEFT SIDE: Glossy + normal map + bump map ----
  auto nmap_mat = std::make_shared<Glossy>(
      std::make_shared<ImageTexture>("../../assets/bricks.jpg"), 0.3, 0.5);
  nmap_mat->set_normal_map(
      std::make_shared<ImageTexture>("../../assets/bricks_normal.jpg", true), 5.0);
  nmap_mat->set_bump_map(
      std::make_shared<ImageTexture>("../../assets/bricks_bump.jpg", true), 10.0);

  // Sphere with normal/bump maps
//   scene.world.add(std::make_shared<Sphere>(Vec3(0, 0, 0), 10, nmap_mat));

  // Left floor
  scene.world.add(std::make_shared<Triangle>(
      Vec3(-15, 0, -15), Vec3(0, 0, 15), Vec3(0, 0, -15), nmap_mat,
      Vec3(0,0,0), Vec3(tile,tile,0), Vec3(tile,0,0),
      Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
  scene.world.add(std::make_shared<Triangle>(
      Vec3(-15, 0, -15), Vec3(-15, 0, 15), Vec3(0, 0, 15), nmap_mat,
      Vec3(0,0,0), Vec3(0,tile,0), Vec3(tile,tile,0),
      Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));

  // Left wall
  scene.world.add(std::make_shared<Triangle>(
      Vec3(-15, 0, -5), Vec3(0, 0, -5), Vec3(0, 8, -5), nmap_mat,
      Vec3(0,0,0), Vec3(tile,0,0), Vec3(tile,tile*0.5f,0),
      Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));
  scene.world.add(std::make_shared<Triangle>(
      Vec3(-15, 0, -5), Vec3(0, 8, -5), Vec3(-15, 8, -5), nmap_mat,
      Vec3(0,0,0), Vec3(tile,tile*0.5f,0), Vec3(0,tile*0.5f,0),
      Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));

  // ---- RIGHT SIDE: Glossy flat (no normal/bump map) ----
  auto flat_mat = std::make_shared<Glossy>(
      std::make_shared<ImageTexture>("../../assets/bricks.jpg"), 0.3, 0.5);

  // Right floor
  scene.world.add(std::make_shared<Triangle>(
      Vec3(0, 0, -15), Vec3(15, 0, 15), Vec3(15, 0, -15), flat_mat,
      Vec3(0,0,0), Vec3(tile,tile,0), Vec3(tile,0,0),
      Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
  scene.world.add(std::make_shared<Triangle>(
      Vec3(0, 0, -15), Vec3(0, 0, 15), Vec3(15, 0, 15), flat_mat,
      Vec3(0,0,0), Vec3(0,tile,0), Vec3(tile,tile,0),
      Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));

  // Right wall
  scene.world.add(std::make_shared<Triangle>(
      Vec3(0, 0, -5), Vec3(15, 0, -5), Vec3(15, 8, -5), flat_mat,
      Vec3(0,0,0), Vec3(tile,0,0), Vec3(tile,tile*0.5f,0),
      Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));
  scene.world.add(std::make_shared<Triangle>(
      Vec3(0, 0, -5), Vec3(15, 8, -5), Vec3(0, 8, -5), flat_mat,
      Vec3(0,0,0), Vec3(tile,tile*0.5f,0), Vec3(0,tile*0.5f,0),
      Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));

  // ---- Area light ----
  auto light_mat = std::make_shared<DiffuseLight>(Vec3(1.0, 0.95, 0.8) * 8.0);
  scene.world.add(std::make_shared<Sphere>(Vec3(0, 20, 0), 3.0, light_mat));
  scene.lights.push_back(
      std::make_shared<SphereAreaLight>(Vec3(0, 20, 0), 3.0, Vec3(1.0, 0.95, 0.8), 8.0));

  return scene;
}

REGISTER_SCENE("brick_demo", build_brick_demo);