// CUDA path entry point
// Scene is selected by name from the registry (default: "brick_demo")
// Usage: ./render_cuda [scene_name] [output_path]

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include "../core/vec3.h"
#include "../core/camera.h"
#include "../objects/hittable_list.h"
#include "../objects/bvh_node.h"
#include "../renderer/renderer.h"
#include "../utils/image_writer.h"
#include "../textures/hdr_texture.h"
#include "../scenes/scene_registry.h"

// CUDA specific
#include "../renderer/cuda/cuda_renderer.cuh"
#include "../renderer/cuda/cuda_bvh.cuh"

// Light conversion
#include "../lights/directional_light.h"
#include "../lights/area_light.h"

// stbi_loadf is declared via hdr_texture.h / stb_image.h
extern "C" float *stbi_loadf(char const *, int *, int *, int *, int);
extern "C" void stbi_image_free(void *);

static std::vector<GpuLight> convert_lights(const std::vector<std::shared_ptr<Light>> &lights) {
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

static GpuCamera convert_camera(const Camera &cam) {
  GpuCamera gc{};
  gc.origin = {(float)cam.origin.x, (float)cam.origin.y, (float)cam.origin.z};
  gc.lower_left_corner = {(float)cam.lower_left_corner.x, (float)cam.lower_left_corner.y, (float)cam.lower_left_corner.z};
  gc.horizontal = {(float)cam.horizontal.x, (float)cam.horizontal.y, (float)cam.horizontal.z};
  gc.vertical   = {(float)cam.vertical.x, (float)cam.vertical.y, (float)cam.vertical.z};
  gc.lens_radius = (float)cam.lens_radius;
  return gc;
}

int main(int argc, char *argv[]) {
  std::string scene_name  = (argc > 1) ? argv[1] : "brick_demo";
  std::string output_path = (argc > 2) ? argv[2] : "../../image_cuda.png";

  std::cout << "Available scenes:";
  for (auto &s : SceneRegistry::list()) std::cout << " " << s;
  std::cout << "\n";

  std::cout << "Building scene: " << scene_name << "\n";
  SceneDesc desc = SceneRegistry::build(scene_name);

  if (desc.world.objects.empty()) {
    std::cerr << "Scene has no objects, aborting.\n";
    return 1;
  }

  int W   = desc.width;
  int H   = desc.height;
  int spp = desc.samples_per_pixel;

  // Build BVH (needed for flat-scene conversion)
  auto bvh = desc.build_bvh();
  Camera camera = desc.build_camera();

  // ---- Convert scene to flat GPU arrays ----
  std::cout << "Converting scene to GPU format...\n";
  FlatScene flat = build_flat_scene(*bvh);

  std::vector<GpuLight> gpu_lights = convert_lights(desc.lights);

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

  // ---- Load HDR sky for GPU ----
  host_scene.sky.enabled   = false;
  host_scene.sky.data      = nullptr;
  host_scene.sky.width     = 0;
  host_scene.sky.height    = 0;
  host_scene.sky.intensity = desc.sky_intensity;

  float *sky_data = nullptr;
  int sky_w = 0, sky_h = 0, sky_c = 0;
  if (!desc.sky_hdr_path.empty()) {
    sky_data = stbi_loadf(desc.sky_hdr_path.c_str(), &sky_w, &sky_h, &sky_c, 3);
    if (sky_data) {
      host_scene.sky.enabled   = true;
      host_scene.sky.data      = sky_data;
      host_scene.sky.width     = sky_w;
      host_scene.sky.height    = sky_h;
      std::cout << "HDR sky loaded: " << sky_w << "x" << sky_h << "\n";
    } else {
      std::cerr << "Warning: Could not load " << desc.sky_hdr_path << ", using gradient fallback\n";
    }
  }

  CudaRenderParams params{};
  params.width  = W;
  params.height = H;
  params.samples_per_pixel = spp;
  params.max_depth = desc.max_depth;
  params.camera = convert_camera(camera);

  std::cout << "Rendering " << W << "x" << H << " @ " << spp << " spp on GPU...\n";
  std::vector<float> fb;
  cuda_render(params, host_scene, gpu_lights, fb);

  if (sky_data) stbi_image_free(sky_data);

  // Convert float buffer to Vec3 framebuffer
  std::vector<Vec3> framebuffer(W * H);
  for (int i = 0; i < W * H; ++i)
    framebuffer[i] = Vec3(fb[i*3+0], fb[i*3+1], fb[i*3+2]);

  save_png(output_path, W, H, framebuffer, 1);
  std::cout << "Saved " << output_path << "\n";

  return 0;
}
