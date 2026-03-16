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
#include "../lights/area_light.h"
#include "../lights/directional_light.h"
#include "../textures/solid_color.h"
#include "../textures/image_texture.h"
#include "../utils/image_writer.h"
#include "../renderer/renderer.h"
#include "../renderer/cuda/cuda_renderer.cuh"
#include "../renderer/cuda/cuda_bvh.cuh"

// convert cpu light list to gpu
std::vector<GpuLight> convert_lights(const LightList &lights) {
  std::vector<GpuLight> gpu_lights;
  for (auto &l : lights) {
    GpuLight gl{};
    if (auto *dl = dynamic_cast<DirectionalLight*>(l.get())) {
      gl.type = GpuLightType::DIRECTIONAL;
      gl.direction = {(float)dl->direction.x, (float)dl->direction.y, (float)dl->direction.z};
      gl.color = {(float)dl->color.x, (float)dl->color.y, (float)dl->color.z};
      gl.intensity = (float)dl->intensity;

    } else if (auto *sal = dynamic_cast<SphereAreaLight*>(l.get())) {
      gl.type = GpuLightType::SPHERE_AREA;
      gl.position = {(float)sal->center.x, (float)sal->center.y, (float)sal->center.z};
      gl.radius = (float)sal->radius;
      gl.color = {(float)sal->color.x, (float)sal->color.y, (float)sal->color.z};
      gl.intensity = (float)sal->intensity;
    }
    gpu_lights.push_back(gl);
  }
  return gpu_lights;
}

// Convert CPU to GPU camera
GpuCamera convert_camera(const Camera &cam) {
  GpuCamera gc{};
  gc.origin = {(float)cam.origin.x, (float)cam.origin.y, (float)cam.origin.z};
  gc.lower_left_corner = {(float)cam.lower_left_corner.x, (float)cam.lower_left_corner.y, (float)cam.lower_left_corner.z};
  gc.horizontal = {(float)cam.horizontal.x, (float)cam.horizontal.y, (float)cam.horizontal.z};
  gc.vertical   = {(float)cam.vertical.x, (float)cam.vertical.y, (float)cam.vertical.z};
  gc.lens_radius = (float)cam.lens_radius;
  return gc;
}

int main() {
  int width = 1080;
  int height = 720;
  int samples_per_pixel = 10;
  int max_depth = 20;

  // build scene (identical to CPU main.cpp)
  HittableList world;
  LightList    lights;

  auto mat1 = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(1, 0, 0)));
  auto mat2 = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0, 1, 0) * 0.2));

  // use 2 triangles instead of an infinite plane
  auto floor_mesh = std::make_shared<HittableList>();
  float s = 500.0f;
  auto t1 = std::make_shared<Triangle>(Vec3(-s,0,-s), Vec3(s,0,s), Vec3(s,0,-s), mat1);
  auto t2 = std::make_shared<Triangle>(Vec3(-s,0,-s), Vec3(-s,0,s), Vec3(s,0,s), mat1);
  world.add(t1);
  world.add(t2);

  auto obj_list = std::make_shared<HittableList>();
  auto triangles = load_obj_triangle("../../assets/models/grass/Untitled.obj", mat2, 0.5, Vec3(0,0,0));
  for (auto t : triangles) obj_list->add(t);
  auto obj_bvh = std::make_shared<BVHNode>(obj_list->objects, 0, obj_list->objects.size());
  world.add(std::make_shared<Rotate>(obj_bvh, 0, 0, 0));

  AABB debug_box;
  obj_bvh->bounding_box(debug_box);
  std::cout << "Mesh min: " << debug_box.minimum.x << " " << debug_box.minimum.y << " " << debug_box.minimum.z << "\n";
  std::cout << "Mesh max: " << debug_box.maximum.x << " " << debug_box.maximum.y << " " << debug_box.maximum.z << "\n";

  auto light_mat = std::make_shared<DiffuseLight>(Vec3(1.0, 0.9, 0.7) * 10.0);
  world.add(std::make_shared<Sphere>(Vec3(0,15,0), 3.0, light_mat));
  lights.push_back(std::make_shared<SphereAreaLight>(Vec3(0,15,0), 3.0, Vec3(1.0,0.9,0.7), 10.0));

  // world.add(std::make_shared<Sphere>(Vec3(0,-1000,0), 1000.0, mat1));

  lights.push_back(std::make_shared<DirectionalLight>(Vec3(-1,1,1), Vec3(1,1,1), 1.0));

  BVHNode world_bvh(world.objects, 0, world.objects.size());

  // camera
  double aspect = double(width) / height;
  Vec3 lookfrom(0, 20, 30);
  Vec3 lookat(0, 0, 0);
  Vec3 vup(0, 1, 0);
  Camera camera(lookfrom, lookat, vup, 90, aspect, 0.0, (lookfrom - lookat).norm());

  // convert scene to flat GPU arrays
  std::cout << "Converting scene to GPU format...\n";
  FlatScene flat = build_flat_scene(world_bvh);

  std::vector<GpuLight> gpu_lights = convert_lights(lights);

  GpuScene host_scene{};
  host_scene.primitives = flat.primitives.data();
  host_scene.num_primitives = (int)flat.primitives.size();
  host_scene.bvh_nodes = flat.bvh_nodes.data();
  host_scene.num_bvh_nodes = (int)flat.bvh_nodes.size();
  host_scene.bvh_root = flat.root;
  host_scene.materials = flat.materials.data();
  host_scene.num_materials = (int)flat.materials.size();
  host_scene.lights = nullptr; // uploaded separately
  host_scene.num_lights = (int)gpu_lights.size();
  // textures
  host_scene.tex_data      = flat.tex_data.empty()  ? nullptr : flat.tex_data.data();
  host_scene.textures      = flat.textures.empty()  ? nullptr : flat.textures.data();
  host_scene.num_textures  = (int)flat.textures.size();

  CudaRenderParams params{};
  params.width = width;
  params.height = height;
  params.samples_per_pixel = samples_per_pixel;
  params.max_depth = max_depth;
  params.camera = convert_camera(camera);

  std::cout << "Rendering on GPU...\n";
  std::vector<float> fb;
  cuda_render(params, host_scene, gpu_lights, fb);


  std::vector<Vec3> framebuffer(width * height);
  for (int i = 0; i < width * height; ++i)
    framebuffer[i] = Vec3(fb[i*3+0], fb[i*3+1], fb[i*3+2]);

  save_png("../../image_cuda.png", width, height, framebuffer, 1);
  std::cout << "Saved image_cuda.png\n";

  return 0;
}