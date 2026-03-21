#pragma once
// scene_definition.h
// Single source of truth for the cathedral scene.
// Include this in both main.cpp and main_cuda.cu.
// Requires HittableList, LightList, and all material/object headers.

#include "../objects/triangle.h"
#include "../objects/sphere.h"
#include "../materials/lambertian.h"
#include "../materials/metal.h"
#include "../materials/dielectric.h"
#include "../materials/glossy.h"
#include "../materials/diffuse_light.h"
#include "../lights/area_light.h"
#include "../textures/solid_color.h"
#include "../textures/image_texture.h"
#include "../textures/hdr_texture.h"
#include "../objects/hittable_list.h"
#include "../renderer/volumetric.h"
#include "../renderer/renderer.h"   // for g_sky_texture, g_volume etc.

// ---- Geometry helpers ----

static void scene_add_quad(HittableList &world, Vec3 a, Vec3 b, Vec3 c, Vec3 d, std::shared_ptr<Material> mat, Vec3 uv_a, Vec3 uv_b, Vec3 uv_c, Vec3 uv_d, Vec3 normal) {
  world.add(std::make_shared<Triangle>(a, b, c, mat, uv_a, uv_b, uv_c, normal, normal, normal));
  world.add(std::make_shared<Triangle>(a, c, d, mat, uv_a, uv_c, uv_d, normal, normal, normal));
}

static void scene_add_box(HittableList &world, Vec3 mn, Vec3 mx, std::shared_ptr<Material> mat) {
  Vec3 uv00(0,0,0), uv10(1,0,0), uv11(1,1,0), uv01(0,1,0);
  // +Z front
  scene_add_quad(world, {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z}, mat, uv00, uv10, uv11, uv01, {0,0,1});
  // -Z back
  scene_add_quad(world, {mx.x,mn.y,mn.z},{mn.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z}, mat, uv00, uv10, uv11, uv01, {0,0,-1});
  // -X left
  scene_add_quad(world, {mn.x,mn.y,mn.z},{mn.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mn.x,mx.y,mn.z}, mat, uv00, uv10, uv11, uv01, {-1,0,0});
  // +X right
  scene_add_quad(world, {mx.x,mn.y,mx.z},{mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z}, mat, uv00, uv10, uv11, uv01, {1,0,0});
  // +Y top
  scene_add_quad(world, {mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},{mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z}, mat, uv00, uv10, uv11, uv01, {0,1,0});
  // -Y bottom
  scene_add_quad(world, {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},{mn.x,mn.y,mx.z}, mat, uv00, uv10, uv11, uv01, {0,-1,0});
}

// ---- Scene dimensions (shared constants) ----
constexpr double ROOM_W  = 16.0;
constexpr double ROOM_H  = 14.0;
constexpr double ROOM_D  = 40.0;
constexpr double HALF_W  = ROOM_W / 2.0;
constexpr double HALF_D  = ROOM_D / 2.0;
constexpr double WIN_HW  = 1.5;   // window half-width
constexpr double WIN_HD  = 2.0;   // window half-depth
constexpr double LIGHT_INTENSITY = 40.0;
static const double WIN_CENTERS[] = {-10.0, 0.0, 10.0};

// ---- Asset path prefix (override before including if needed) ----
#ifndef ASSET_PATH
#define ASSET_PATH "../../assets/"
#endif

struct SceneResult {
  Camera camera;
  double exposure = 1.4;

  SceneResult() = default;
  SceneResult(const Camera& cam, double exp = 1.4) : camera(cam), exposure(exp) {}
};

// Build the cathedral scene. Populates world, lights, g_volume, g_sky_texture.
// Returns camera and exposure.
inline SceneResult build_cathedral_scene(HittableList &world, LightList &lights, int width, int height) {
  // HDR sky
  auto sky = std::make_shared<HdrTexture>(std::string(ASSET_PATH) + "sky.hdr");
  if (sky->data) {
    g_sky_texture  = sky;
    g_sky_intensity = 0.5f;
  }

  // Materials
  auto stone_tex = std::make_shared<ImageTexture>(std::string(ASSET_PATH) + "bricks.jpg");
  auto stone_mat = std::make_shared<Glossy>(stone_tex, 0.8, 0.04);
  stone_mat->set_normal_map(std::make_shared<ImageTexture>(std::string(ASSET_PATH) + "bricks_normal.jpg", true), 2.0);
  stone_mat->set_bump_map(std::make_shared<ImageTexture>(std::string(ASSET_PATH) + "bricks_bump.jpg", true), 3.0);

  auto floor_mat = std::make_shared<Glossy>(std::make_shared<SolidColor>(Vec3(0.08, 0.07, 0.06)), 0.4, 0.12);

  auto ceil_mat = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.12, 0.11, 0.10)));

  auto pillar_mat = std::make_shared<Glossy>(std::make_shared<SolidColor>(Vec3(0.18, 0.17, 0.15)), 0.6, 0.06);

  // Floor
  float tf = 6.0f;
  scene_add_quad(world, {-HALF_W, 0, -HALF_D}, {HALF_W, 0, -HALF_D}, {HALF_W, 0, HALF_D},   {-HALF_W, 0, HALF_D}, floor_mat, {0,0,0},{tf,0,0},{tf,tf,0},{0,tf,0}, {0,1,0});

  // Ceiling panels around skylight openings
  double cy = ROOM_H;
  auto add_ceil = [&](double x0, double z0, double x1, double z1) {
    scene_add_quad(world, {x0,cy,z0},{x1,cy,z0},{x1,cy,z1},{x0,cy,z1}, ceil_mat, {0,0,0},{1,0,0},{1,1,0},{0,1,0}, {0,-1,0});
  };

  add_ceil(-HALF_W, -HALF_D, HALF_W, -12);
  add_ceil(-HALF_W, -8, HALF_W, -2);
  add_ceil(-HALF_W, 2, HALF_W, 8);
  add_ceil(-HALF_W, 12, HALF_W, HALF_D);

  for (double wz : WIN_CENTERS) {
    add_ceil(-HALF_W, wz - WIN_HD, -WIN_HW, wz + WIN_HD);
    add_ceil(WIN_HW,  wz - WIN_HD,  HALF_W, wz + WIN_HD);
  }

  // Walls
  float tw = 4.0f;
  // Left (-X)
  scene_add_quad(world, {-HALF_W,0,-HALF_D},{-HALF_W,0,HALF_D}, {-HALF_W,ROOM_H,HALF_D},{-HALF_W,ROOM_H,-HALF_D}, stone_mat, {0,0,0},{tw,0,0},{tw,tw*0.5f,0},{0,tw*0.5f,0}, {1,0,0});
  // Right (+X)
  scene_add_quad(world, {HALF_W,0,HALF_D},{HALF_W,0,-HALF_D}, {HALF_W,ROOM_H,-HALF_D},{HALF_W,ROOM_H,HALF_D}, stone_mat, {0,0,0},{tw,0,0},{tw,tw*0.5f,0},{0,tw*0.5f,0}, {-1,0,0});
  // Back (-Z)
  scene_add_quad(world, {HALF_W,0,-HALF_D},{-HALF_W,0,-HALF_D}, {-HALF_W,ROOM_H,-HALF_D},{HALF_W,ROOM_H,-HALF_D}, stone_mat, {0,0,0},{tw,0,0},{tw,tw*0.5f,0},{0,tw*0.5f,0}, {0,0,1});
  // Front (+Z)
  scene_add_quad(world, {-HALF_W,0,HALF_D},{HALF_W,0,HALF_D}, {HALF_W,ROOM_H,HALF_D},{-HALF_W,ROOM_H,HALF_D}, stone_mat, {0,0,0},{tw,0,0},{tw,tw*0.5f,0},{0,tw*0.5f,0}, {0,0,-1});

  // Pillars
  double pw = 0.8;
  double px_l = -4.0, px_r = 4.0;
  double pz_positions[] = {-14.0, -7.0, 0.0, 7.0, 14.0};
  for (double pz : pz_positions) {
    scene_add_box(world,
      {px_l - pw/2, 0, pz - pw/2},
      {px_l + pw/2, ROOM_H, pz + pw/2}, pillar_mat);
    scene_add_box(world,
      {px_r - pw/2, 0, pz - pw/2},
      {px_r + pw/2, ROOM_H, pz + pw/2}, pillar_mat);
  }

  // Skylights — emissive spheres above ceiling gaps
  for (double wz : WIN_CENTERS) {
    Vec3 lpos(0, cy + 3.0, wz);
    double lrad = 1.5;
    Vec3 lcol(1.0, 0.92, 0.8);
    auto lmat = std::make_shared<DiffuseLight>(lcol * LIGHT_INTENSITY);
    world.add(std::make_shared<Sphere>(lpos, lrad, lmat));
    lights.push_back(std::make_shared<SphereAreaLight>(
        lpos, lrad, lcol, LIGHT_INTENSITY));
  }

  // Decorative objects
  world.add(std::make_shared<Sphere>(Vec3(0, 1.0, -8), 1.0,
      std::make_shared<Dielectric>(1.5)));

  world.add(std::make_shared<Sphere>(Vec3(-2.5, 0.6, -3), 0.6,
      std::make_shared<Metal>(
          std::make_shared<SolidColor>(Vec3(0.85, 0.65, 0.2)), 0.15)));

  world.add(std::make_shared<Sphere>(Vec3(2.5, 0.5, 2), 0.5,
      std::make_shared<Lambertian>(
          std::make_shared<SolidColor>(Vec3(0.7, 0.1, 0.1)))));

  // Volumetric dusty air
  auto vol = std::make_shared<VolumeRegion>();
  vol->bounds = VolumeAABB{
      Vec3(-HALF_W, 0, -HALF_D),
      Vec3(HALF_W, ROOM_H, HALF_D)};
  // PERFORMANCE: keep sigma_s high enough for visible shafts,
  // but the stochastic integrator keeps it cheap regardless.
  vol->sigma_s = 0.15;
  vol->sigma_a = 0.002;
  vol->g       = 0.5;   // forward scattering toward camera
  g_volume = vol;

  // Camera
  double aspect = double(width) / height;
  Vec3 lookfrom(0, 3.0, 14);
  Vec3 lookat(0, 10.0, 0.0);
  double focus_dist = (lookfrom - lookat).norm();

  Camera cam(lookfrom, lookat, Vec3(0,1,0), 70, aspect, 0.0, focus_dist);
  SceneResult result(cam, 1.4);
  return result;
}