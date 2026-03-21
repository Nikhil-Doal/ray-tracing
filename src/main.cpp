#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <vector>
#include "scene_definition.h"
#include "../core/vec3.h"
#include "../objects/hittable_list.h"
#include "../objects/bvh_node.h"
#include "../renderer/renderer.h"
#include "../utils/image_writer.h"

int main() {
  const int width             = 1280;
  const int height            = 720;
  const int samples_per_pixel = 64;
  const int max_depth         = 12;

  HittableList world;
  LightList lights;

  auto scene = build_cathedral_scene(world, lights, width, height);

  std::cout << "Building BVH over " << world.objects.size() << " objects...\n";
  BVHNode world_bvh(world.objects, 0, world.objects.size());

  std::cout << "Rendering cathedral (CPU)...\n";
  std::cout << "  " << width << "x" << height
            << " @ " << samples_per_pixel << " spp\n";
  if (g_volume)
    std::cout << "  Volume: sigma_s=" << g_volume->sigma_s
              << " g=" << g_volume->g << " (stochastic)\n";

  std::vector<Vec3> framebuffer(width * height);
  std::cout << "Thread count: " << std::thread::hardware_concurrency() << "\n";
  render_image(width, height, samples_per_pixel, max_depth,
               scene.camera, world_bvh, lights, framebuffer);

  save_png("../../image.png", width, height, framebuffer,
           samples_per_pixel, scene.exposure);

  std::cout << "Done — saved image.png\n";
  return 0;
}