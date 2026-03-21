// CPU path entry point
// Scene is selected by name from the registry (default: "brick_demo")
// Usage: ./render [scene_name] [output_path]

#include <iostream>
#include <string>
#include <vector>

#include "../core/vec3.h"
#include "../core/camera.h"
#include "../objects/bvh_node.h"
#include "../renderer/renderer.h"
#include "../utils/image_writer.h"
#include "../textures/hdr_texture.h"
#include "../scenes/scene_registry.h"

int main(int argc, char *argv[]) {
  std::string scene_name  = (argc > 1) ? argv[1] : "brick_demo";
  std::string output_path = (argc > 2) ? argv[2] : "../../image.png";

  // List available scenes
  std::cout << "Available scenes:";
  for (auto &s : SceneRegistry::list()) std::cout << " " << s;
  std::cout << "\n";

  std::cout << "Building scene: " << scene_name << "\n";
  SceneDesc desc = SceneRegistry::build(scene_name);

  if (desc.world.objects.empty()) {
    std::cerr << "Scene has no objects, aborting.\n";
    return 1;
  }

  // HDR sky setup
  if (!desc.sky_hdr_path.empty()) {
    g_sky_texture   = std::make_shared<HdrTexture>(desc.sky_hdr_path);
    g_sky_intensity = desc.sky_intensity;
  }

  // Build BVH
  auto bvh = desc.build_bvh();
  Camera camera = desc.build_camera();

  int W = desc.width;
  int H = desc.height;
  int spp = desc.samples_per_pixel;

  std::cout << "Rendering " << W << "x" << H << " @ " << spp << " spp, depth " << desc.max_depth << "\n";

  std::vector<Vec3> framebuffer(W * H);
  render_image(W, H, spp, desc.max_depth, camera, *bvh, desc.lights, framebuffer);
  save_png(output_path, W, H, framebuffer, spp);

  std::cout << "Saved " << output_path << "\n";
  return 0;
}