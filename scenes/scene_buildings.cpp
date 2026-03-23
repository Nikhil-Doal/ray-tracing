#include "scene_registry.h"
#include "../objects/sphere.h"
#include "../objects/triangle.h"
#include "../objects/obj_loader.h"
#include "../materials/lambertian.h"
#include "../materials/glossy.h"
#include "../materials/diffuse_light.h"
#include "../textures/image_texture.h"
#include "../textures/solid_color.h"
#include "../lights/directional_light.h"
#include "../lights/area_light.h"

static SceneDesc build_buildings() {
  SceneDesc scene;

  scene.width  = 1920;
  scene.height = 1080;
  scene.samples_per_pixel = 128;
  scene.max_depth = 20;

  // Camera — street-level perspective, close enough to see material detail
  // Buildings sit at Y:[0, ~3.6] after offset, spread in a 2x2 grid
  scene.lookfrom = Vec3(22, 8, 28);
  scene.lookat   = Vec3(0, 2, 0);
  scene.vfov     = 50.0;
  scene.aperture = 0.0;

  // HDR environment
  scene.sky_hdr_path  = "../../assets/sky.hdr";
  scene.sky_intensity = 1.2f;

  // Sun — warm directional light from the side for good material visibility
  scene.lights.push_back(std::make_shared<DirectionalLight>(
      Vec3(-0.5, -1.0, -0.3), Vec3(1.0, 0.95, 0.85), 2.5));

  // Fallback material (won't be used if MTLs load correctly)
  auto fallback = std::make_shared<Lambertian>(
      std::make_shared<SolidColor>(Vec3(0.5, 0.5, 0.5)));

  // ---- Ground plane (grass-textured) ----
  auto ground_mat = std::make_shared<Lambertian>(
      std::make_shared<ImageTexture>("../../assets/obj/textures/Grass_col.JPG"));
  float gs = 80.0f;  // ground half-size
  float tile = 8.0f; // UV tiling

  scene.world.add(std::make_shared<Triangle>(
      Vec3(-gs, 0, -gs), Vec3(gs, 0, -gs), Vec3(gs, 0, gs), ground_mat,
      Vec3(0,0,0), Vec3(tile,0,0), Vec3(tile,tile,0),
      Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
  scene.world.add(std::make_shared<Triangle>(
      Vec3(-gs, 0, -gs), Vec3(gs, 0, gs), Vec3(-gs, 0, gs), ground_mat,
      Vec3(0,0,0), Vec3(tile,tile,0), Vec3(0,tile,0),
      Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));

  // ---- Load buildings ----
  // Each building is ~12.4 wide (X), ~23 deep (Z), base at Y≈3.5
  // Offset Y by -3.5 to place them on the ground plane
  // Buildings are spaced in a 2x2 grid with a ~2-unit gap (street)

  std::string obj_dir = "../../assets/obj/";
  double yoff = -3.5;
  double spacing_x = 15.0;  // center-to-center X spacing
  double spacing_z = 26.0;  // center-to-center Z spacing

  // Front row (closer to camera, negative Z)
  auto tris1 = load_obj_triangle(obj_dir + "Residential Buildings 001.obj", fallback, 1.0,
      Vec3(-spacing_x * 0.5, yoff, -spacing_z * 0.5));
  for (auto &t : tris1) scene.world.add(t);

  auto tris2 = load_obj_triangle(obj_dir + "Residential Buildings 003.obj", fallback, 1.0,
      Vec3(spacing_x * 0.5, yoff, -spacing_z * 0.5));
  for (auto &t : tris2) scene.world.add(t);

  // Back row (further from camera, positive Z)
  auto tris3 = load_obj_triangle(obj_dir + "Residential Buildings 005.obj", fallback, 1.0,
      Vec3(-spacing_x * 0.5, yoff, spacing_z * 0.5));
  for (auto &t : tris3) scene.world.add(t);

  auto tris4 = load_obj_triangle(obj_dir + "Residential Buildings 007.obj", fallback, 1.0,
      Vec3(spacing_x * 0.5, yoff, spacing_z * 0.5));
  for (auto &t : tris4) scene.world.add(t);

  return scene;
}

REGISTER_SCENE("buildings", build_buildings);