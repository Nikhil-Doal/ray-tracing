#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <vector>
#include <unordered_map>

#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../core/camera.h"
#include "../objects/sphere.h"
#include "../objects/plane.h"
#include "../objects/hittable_list.h"
#include "../objects/bvh_node.h"
#include "../objects/obj_loader.h"
#include "../objects/rotate.h"
#include "../objects/triangle.h"
#include "../materials/material.h"
#include "../materials/lambertian.h"
#include "../materials/metal.h"
#include "../materials/dielectric.h"
#include "../materials/diffuse_light.h"
#include "../materials/glossy.h"
#include "../lights/area_light.h"
#include "../lights/directional_light.h"
#include "../textures/solid_color.h"
#include "../textures/image_texture.h"
#include "../textures/hdr_texture.h"
#include "../utils/image_writer.h"
#include "../renderer/renderer.h"
#include "../renderer/cuda/cuda_renderer.cuh"
#include "../renderer/cuda/cuda_bvh.cuh"

// ---- helpers: same quad/box builders as main.cpp ----

static void add_quad(HittableList &world,
  Vec3 a, Vec3 b, Vec3 c, Vec3 d,
  std::shared_ptr<Material> mat,
  Vec3 uv_a, Vec3 uv_b, Vec3 uv_c, Vec3 uv_d,
  Vec3 normal)
{
  world.add(std::make_shared<Triangle>(a, b, c, mat,
    uv_a, uv_b, uv_c, normal, normal, normal));
  world.add(std::make_shared<Triangle>(a, c, d, mat,
    uv_a, uv_c, uv_d, normal, normal, normal));
}

static void add_box(HittableList &world,
  Vec3 mn, Vec3 mx, std::shared_ptr<Material> mat)
{
  Vec3 uv00(0,0,0), uv10(1,0,0), uv11(1,1,0), uv01(0,1,0);
  // +Z
  add_quad(world,
    Vec3(mn.x,mn.y,mx.z), Vec3(mx.x,mn.y,mx.z),
    Vec3(mx.x,mx.y,mx.z), Vec3(mn.x,mx.y,mx.z),
    mat, uv00, uv10, uv11, uv01, Vec3(0,0,1));
  // -Z
  add_quad(world,
    Vec3(mx.x,mn.y,mn.z), Vec3(mn.x,mn.y,mn.z),
    Vec3(mn.x,mx.y,mn.z), Vec3(mx.x,mx.y,mn.z),
    mat, uv00, uv10, uv11, uv01, Vec3(0,0,-1));
  // -X
  add_quad(world,
    Vec3(mn.x,mn.y,mn.z), Vec3(mn.x,mn.y,mx.z),
    Vec3(mn.x,mx.y,mx.z), Vec3(mn.x,mx.y,mn.z),
    mat, uv00, uv10, uv11, uv01, Vec3(-1,0,0));
  // +X
  add_quad(world,
    Vec3(mx.x,mn.y,mx.z), Vec3(mx.x,mn.y,mn.z),
    Vec3(mx.x,mx.y,mn.z), Vec3(mx.x,mx.y,mx.z),
    mat, uv00, uv10, uv11, uv01, Vec3(1,0,0));
  // +Y
  add_quad(world,
    Vec3(mn.x,mx.y,mx.z), Vec3(mx.x,mx.y,mx.z),
    Vec3(mx.x,mx.y,mn.z), Vec3(mn.x,mx.y,mn.z),
    mat, uv00, uv10, uv11, uv01, Vec3(0,1,0));
  // -Y
  add_quad(world,
    Vec3(mn.x,mn.y,mn.z), Vec3(mx.x,mn.y,mn.z),
    Vec3(mx.x,mn.y,mx.z), Vec3(mn.x,mn.y,mx.z),
    mat, uv00, uv10, uv11, uv01, Vec3(0,-1,0));
}

// ---- converters ----

std::vector<GpuLight> convert_lights(const LightList &lights) {
  std::vector<GpuLight> gpu_lights;
  for (auto &l : lights) {
    GpuLight gl{};
    if (auto *dl = dynamic_cast<DirectionalLight *>(l.get())) {
      gl.type = GpuLightType::DIRECTIONAL;
      gl.direction = {(float)dl->direction.x, (float)dl->direction.y,
                      (float)dl->direction.z};
      gl.color = {(float)dl->color.x, (float)dl->color.y,
                  (float)dl->color.z};
      gl.intensity = (float)dl->intensity;
    } else if (auto *sal = dynamic_cast<SphereAreaLight *>(l.get())) {
      gl.type = GpuLightType::SPHERE_AREA;
      gl.position = {(float)sal->center.x, (float)sal->center.y,
                     (float)sal->center.z};
      gl.radius = (float)sal->radius;
      gl.color = {(float)sal->color.x, (float)sal->color.y,
                  (float)sal->color.z};
      gl.intensity = (float)sal->intensity;
    }
    gpu_lights.push_back(gl);
  }
  return gpu_lights;
}

GpuCamera convert_camera(const Camera &cam) {
  GpuCamera gc{};
  gc.origin = {(float)cam.origin.x, (float)cam.origin.y,
               (float)cam.origin.z};
  gc.lower_left_corner = {(float)cam.lower_left_corner.x,
                          (float)cam.lower_left_corner.y,
                          (float)cam.lower_left_corner.z};
  gc.horizontal = {(float)cam.horizontal.x, (float)cam.horizontal.y,
                   (float)cam.horizontal.z};
  gc.vertical = {(float)cam.vertical.x, (float)cam.vertical.y,
                 (float)cam.vertical.z};
  gc.lens_radius = (float)cam.lens_radius;
  return gc;
}

int main() {
  // ================================================================
  // Settings
  // ================================================================
  int width = 1280;
  int height = 720;
  int samples_per_pixel = 64;
  int max_depth = 16;
  double exposure = 1.4;

  HittableList world;
  LightList lights;

  // ================================================================
  // Materials — identical to main.cpp
  // ================================================================
  auto stone_tex = std::make_shared<ImageTexture>("../../assets/bricks.jpg");
  auto stone_mat = std::make_shared<Glossy>(stone_tex, 0.8, 0.04);
  stone_mat->set_normal_map(
    std::make_shared<ImageTexture>("../../assets/bricks_normal.jpg", true), 2.0);
  stone_mat->set_bump_map(
    std::make_shared<ImageTexture>("../../assets/bricks_bump.jpg", true), 3.0);

  auto floor_mat = std::make_shared<Glossy>(
    std::make_shared<SolidColor>(Vec3(0.08, 0.07, 0.06)), 0.4, 0.12);
  auto ceil_mat = std::make_shared<Lambertian>(
    std::make_shared<SolidColor>(Vec3(0.12, 0.11, 0.10)));
  auto pillar_mat = std::make_shared<Glossy>(
    std::make_shared<SolidColor>(Vec3(0.18, 0.17, 0.15)), 0.6, 0.06);

  // ================================================================
  // Cathedral geometry — identical to main.cpp
  // ================================================================
  double room_w = 16.0, room_h = 14.0, room_d = 40.0;
  double half_w = room_w / 2.0, half_d = room_d / 2.0;
  float tw = 4.0f, tf = 6.0f;

  // Floor
  add_quad(world,
    Vec3(-half_w, 0, -half_d), Vec3(half_w, 0, -half_d),
    Vec3(half_w, 0, half_d), Vec3(-half_w, 0, half_d),
    floor_mat,
    Vec3(0,0,0), Vec3(tf,0,0), Vec3(tf,tf,0), Vec3(0,tf,0),
    Vec3(0, 1, 0));

  // Ceiling with skylight gaps
  double win_half_w = 1.5, win_half_d = 2.0;
  double cy = room_h;
  auto add_ceil = [&](double x0, double z0, double x1, double z1) {
    add_quad(world,
      Vec3(x0, cy, z0), Vec3(x1, cy, z0),
      Vec3(x1, cy, z1), Vec3(x0, cy, z1),
      ceil_mat,
      Vec3(0,0,0), Vec3(1,0,0), Vec3(1,1,0), Vec3(0,1,0),
      Vec3(0, -1, 0));
  };

  add_ceil(-half_w, -half_d, half_w, -12);
  add_ceil(-half_w, -8, half_w, -2);
  add_ceil(-half_w, 2, half_w, 8);
  add_ceil(-half_w, 12, half_w, half_d);

  double win_centers[] = {-10.0, 0.0, 10.0};
  for (double wz : win_centers) {
    add_ceil(-half_w, wz - win_half_d, -win_half_w, wz + win_half_d);
    add_ceil(win_half_w, wz - win_half_d, half_w, wz + win_half_d);
  }

  // Walls
  add_quad(world,
    Vec3(-half_w, 0, -half_d), Vec3(-half_w, 0, half_d),
    Vec3(-half_w, room_h, half_d), Vec3(-half_w, room_h, -half_d),
    stone_mat,
    Vec3(0,0,0), Vec3(tw,0,0), Vec3(tw,tw*0.5f,0), Vec3(0,tw*0.5f,0),
    Vec3(1, 0, 0));
  add_quad(world,
    Vec3(half_w, 0, half_d), Vec3(half_w, 0, -half_d),
    Vec3(half_w, room_h, -half_d), Vec3(half_w, room_h, half_d),
    stone_mat,
    Vec3(0,0,0), Vec3(tw,0,0), Vec3(tw,tw*0.5f,0), Vec3(0,tw*0.5f,0),
    Vec3(-1, 0, 0));
  add_quad(world,
    Vec3(half_w, 0, -half_d), Vec3(-half_w, 0, -half_d),
    Vec3(-half_w, room_h, -half_d), Vec3(half_w, room_h, -half_d),
    stone_mat,
    Vec3(0,0,0), Vec3(tw,0,0), Vec3(tw,tw*0.5f,0), Vec3(0,tw*0.5f,0),
    Vec3(0, 0, 1));
  add_quad(world,
    Vec3(-half_w, 0, half_d), Vec3(half_w, 0, half_d),
    Vec3(half_w, room_h, half_d), Vec3(-half_w, room_h, half_d),
    stone_mat,
    Vec3(0,0,0), Vec3(tw,0,0), Vec3(tw,tw*0.5f,0), Vec3(0,tw*0.5f,0),
    Vec3(0, 0, -1));

  // Pillars
  double pillar_w = 0.8;
  double pillar_x_left = -4.0, pillar_x_right = 4.0;
  double pillar_z[] = {-14.0, -7.0, 0.0, 7.0, 14.0};
  for (double pz : pillar_z) {
    add_box(world,
      Vec3(pillar_x_left - pillar_w/2, 0, pz - pillar_w/2),
      Vec3(pillar_x_left + pillar_w/2, room_h, pz + pillar_w/2),
      pillar_mat);
    add_box(world,
      Vec3(pillar_x_right - pillar_w/2, 0, pz - pillar_w/2),
      Vec3(pillar_x_right + pillar_w/2, room_h, pz + pillar_w/2),
      pillar_mat);
  }

  // Lights above skylights
  for (double wz : win_centers) {
    Vec3 lpos(0, cy + 3.0, wz);
    double lrad = 1.5;
    Vec3 lcol(1.0, 0.92, 0.8);
    double lint = 25.0;
    auto lmat = std::make_shared<DiffuseLight>(lcol * lint);
    world.add(std::make_shared<Sphere>(lpos, lrad, lmat));
    lights.push_back(std::make_shared<SphereAreaLight>(
      lpos, lrad, lcol, lint));
  }

  // Floor objects
  world.add(std::make_shared<Sphere>(Vec3(0, 1.0, -8), 1.0,
    std::make_shared<Dielectric>(1.5)));
  world.add(std::make_shared<Sphere>(Vec3(-2.5, 0.6, -3), 0.6,
    std::make_shared<Metal>(
      std::make_shared<SolidColor>(Vec3(0.85, 0.65, 0.2)), 0.15)));
  world.add(std::make_shared<Sphere>(Vec3(2.5, 0.5, 2), 0.5,
    std::make_shared<Lambertian>(
      std::make_shared<SolidColor>(Vec3(0.7, 0.1, 0.1)))));

  // ================================================================
  // Build BVH
  // ================================================================
  std::cout << "Building BVH over " << world.objects.size() << " objects...\n";
  BVHNode world_bvh(world.objects, 0, world.objects.size());

  // ================================================================
  // Camera
  // ================================================================
  double aspect = double(width) / height;
  Vec3 lookfrom(0, 4.0, 16);
  Vec3 lookat(0, 6.0, -5);
  double focus_dist = (lookfrom - lookat).norm();
  Camera camera(lookfrom, lookat, Vec3(0,1,0), 70, aspect, 0.0, focus_dist);

  // ================================================================
  // Flatten to GPU
  // ================================================================
  std::cout << "Converting scene to GPU format...\n";
  FlatScene flat = build_flat_scene(world_bvh);
  std::vector<GpuLight> gpu_lights = convert_lights(lights);

  GpuScene host_scene{};
  host_scene.primitives     = flat.primitives.data();
  host_scene.num_primitives = (int)flat.primitives.size();
  host_scene.bvh_nodes      = flat.bvh_nodes.data();
  host_scene.num_bvh_nodes  = (int)flat.bvh_nodes.size();
  host_scene.bvh_root       = flat.root;
  host_scene.materials      = flat.materials.data();
  host_scene.num_materials  = (int)flat.materials.size();
  host_scene.lights         = nullptr;
  host_scene.num_lights     = (int)gpu_lights.size();
  host_scene.tex_data       = flat.tex_data.empty() ? nullptr : flat.tex_data.data();
  host_scene.textures       = flat.textures.empty() ? nullptr : flat.textures.data();
  host_scene.num_textures   = (int)flat.textures.size();

  // HDR sky
  host_scene.sky.enabled   = false;
  host_scene.sky.data      = nullptr;
  host_scene.sky.width     = 0;
  host_scene.sky.height    = 0;
  host_scene.sky.intensity = 2.0f;

  int sky_w, sky_h, sky_c;
  float *sky_data = stbi_loadf("../../assets/sky.hdr", &sky_w, &sky_h, &sky_c, 3);
  if (sky_data) {
    host_scene.sky.enabled = true;
    host_scene.sky.data    = sky_data;
    host_scene.sky.width   = sky_w;
    host_scene.sky.height  = sky_h;
    std::cout << "HDR sky: " << sky_w << "x" << sky_h
              << " @ " << host_scene.sky.intensity << "x\n";
  } else {
    std::cerr << "Warning: sky.hdr not found, using fallback gradient\n";
  }

  // Volumetric medium — dusty cathedral air
  host_scene.volume.enabled  = true;
  host_scene.volume.aabb_min = {(float)-half_w, 0.0f, (float)-half_d};
  host_scene.volume.aabb_max = {(float)half_w, (float)room_h, (float)half_d};
  host_scene.volume.sigma_s  = 0.06f;
  host_scene.volume.sigma_a  = 0.004f;
  host_scene.volume.g        = 0.75f;

  // ================================================================
  // Render
  // ================================================================
  CudaRenderParams params{};
  params.width             = width;
  params.height            = height;
  params.samples_per_pixel = samples_per_pixel;
  params.max_depth         = max_depth;
  params.camera            = convert_camera(camera);

  std::cout << "Rendering cathedral (GPU)...\n";
  std::cout << "  " << width << "x" << height
            << " @ " << samples_per_pixel << " spp\n";
  std::cout << "  Volume: sigma_s=" << host_scene.volume.sigma_s
            << " sigma_a=" << host_scene.volume.sigma_a
            << " g=" << host_scene.volume.g << "\n";

  std::vector<float> fb;
  cuda_render(params, host_scene, gpu_lights, fb);

  if (sky_data) stbi_image_free(sky_data);

  std::vector<Vec3> framebuffer(width * height);
  for (int i = 0; i < width * height; ++i)
    framebuffer[i] = Vec3(fb[i*3], fb[i*3+1], fb[i*3+2]);

  save_png("../../image_cuda.png", width, height, framebuffer, 1, exposure);
  std::cout << "Saved image_cuda.png\n";
  return 0;
}
