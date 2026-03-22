#include "scene_registry.h"
#include "../objects/sphere.h"
#include "../objects/triangle.h"
#include "../objects/plane.h"
#include "../materials/lambertian.h"
#include "../materials/metal.h"
#include "../materials/glossy.h"
#include "../materials/diffuse_light.h"
#include "../lights/area_light.h"
#include "../lights/directional_light.h"
#include "../textures/image_texture.h"
#include "../textures/solid_color.h"

// ─────────────────────────────────────────────────────────────────────────────
// Normal-map test scene
//
// Layout (viewed from camera):
//
//   Back row (spheres):   Lambertian  |  Metal  |  Glossy
//                         + normal/bump  + normal   + normal/bump
//
//   Front row (spheres):  Lambertian  |  Metal  |  Glossy
//                         flat (no maps) flat      flat
//
//   Floor: two triangles with normal+bump maps (same brick textures as brick_demo)
//
// This lets you visually compare mapped vs flat for each material type,
// and verifies sphere TBN is working correctly.
// ─────────────────────────────────────────────────────────────────────────────

static SceneDesc build_normal_map_test() {
  SceneDesc scene;

  scene.width  = 1280;
  scene.height = 720;
  scene.samples_per_pixel = 128;
  scene.max_depth = 20;

  scene.lookfrom = Vec3(0, 5, 14);
  scene.lookat   = Vec3(0, 1.5, 0);
  scene.vfov     = 55.0;
  scene.aperture = 0.0;

  scene.sky_hdr_path  = "../../assets/sky.hdr";
  scene.sky_intensity = 0.8f;

  // ── Shared textures ──────────────────────────────────────────────────────
  auto brick_diffuse = std::make_shared<ImageTexture>("../../assets/bricks.jpg");
  auto brick_normal  = std::make_shared<ImageTexture>("../../assets/bricks_normal.jpg", true);
  auto brick_bump    = std::make_shared<ImageTexture>("../../assets/bricks_bump.jpg", true);

  // ── Back row: normal-mapped spheres (y=2, z=-2) ─────────────────────────
  double back_z  = -2.0;
  double front_z =  3.0;
  double sphere_r = 1.5;

  // Lambertian + normal + bump
  {
    auto mat = std::make_shared<Lambertian>(brick_diffuse);
    mat->set_normal_map(brick_normal, 3.0);
    mat->set_bump_map(brick_bump, 6.0);
    scene.world.add(std::make_shared<Sphere>(Vec3(-4.5, sphere_r, back_z), sphere_r, mat));
  }

  // Metal + normal map
  {
    auto mat = std::make_shared<Metal>(brick_diffuse, 0.1);
    mat->set_normal_map(brick_normal, 3.0);
    scene.world.add(std::make_shared<Sphere>(Vec3(0, sphere_r, back_z), sphere_r, mat));
  }

  // Glossy + normal + bump
  {
    auto mat = std::make_shared<Glossy>(brick_diffuse, 0.25, 0.5);
    mat->set_normal_map(brick_normal, 3.0);
    mat->set_bump_map(brick_bump, 6.0);
    scene.world.add(std::make_shared<Sphere>(Vec3(4.5, sphere_r, back_z), sphere_r, mat));
  }

  // ── Front row: flat (no normal/bump maps) spheres (y=1.2, z=3) ─────────
  double small_r = 1.2;

  // Lambertian flat
  {
    auto mat = std::make_shared<Lambertian>(brick_diffuse);
    scene.world.add(std::make_shared<Sphere>(Vec3(-4.5, small_r, front_z), small_r, mat));
  }

  // Metal flat
  {
    auto mat = std::make_shared<Metal>(brick_diffuse, 0.1);
    scene.world.add(std::make_shared<Sphere>(Vec3(0, small_r, front_z), small_r, mat));
  }

  // Glossy flat
  {
    auto mat = std::make_shared<Glossy>(brick_diffuse, 0.25, 0.5);
    scene.world.add(std::make_shared<Sphere>(Vec3(4.5, small_r, front_z), small_r, mat));
  }

  // ── Floor: two triangles with normal + bump maps ────────────────────────
  float tile = 1.0f;
  auto floor_mat = std::make_shared<Glossy>(brick_diffuse, 0.4, 0.3);
  floor_mat->set_normal_map(brick_normal, 5.0);
  floor_mat->set_bump_map(brick_bump, 8.0);

  scene.world.add(std::make_shared<Triangle>(
      Vec3(-12, 0, -8), Vec3(12, 0, 10), Vec3(12, 0, -8), floor_mat,
      Vec3(0,0,0), Vec3(tile*2, tile*2, 0), Vec3(tile*2, 0, 0),
      Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
  scene.world.add(std::make_shared<Triangle>(
      Vec3(-12, 0, -8), Vec3(-12, 0, 10), Vec3(12, 0, 10), floor_mat,
      Vec3(0,0,0), Vec3(0, tile*2, 0), Vec3(tile*2, tile*2, 0),
      Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));

  // ── Back wall: flat lambertian (neutral backdrop) ───────────────────────
  auto wall_mat = std::make_shared<Lambertian>(
      std::make_shared<SolidColor>(Vec3(0.4, 0.4, 0.45)));
  scene.world.add(std::make_shared<Triangle>(
      Vec3(-12, 0, -8), Vec3(12, 0, -8), Vec3(12, 10, -8), wall_mat,
      Vec3(0,0,0), Vec3(1,0,0), Vec3(1,1,0),
      Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));
  scene.world.add(std::make_shared<Triangle>(
      Vec3(-12, 0, -8), Vec3(12, 10, -8), Vec3(-12, 10, -8), wall_mat,
      Vec3(0,0,0), Vec3(1,1,0), Vec3(0,1,0),
      Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));

  // ── Lighting ────────────────────────────────────────────────────────────
  // Directional fill light
  scene.lights.push_back(std::make_shared<DirectionalLight>(
      Vec3(-1, -1, -0.5), Vec3(1.0, 0.95, 0.9), 1.5));

  // Overhead area light for soft shadows and specular highlights
  auto light_mat = std::make_shared<DiffuseLight>(Vec3(1.0, 0.97, 0.9) * 10.0);
  scene.world.add(std::make_shared<Sphere>(Vec3(0, 15, 2), 2.5, light_mat));
  scene.lights.push_back(
      std::make_shared<SphereAreaLight>(Vec3(0, 15, 2), 2.5, Vec3(1.0, 0.97, 0.9), 10.0));

  return scene;
}

REGISTER_SCENE("normal_map_test", build_normal_map_test);