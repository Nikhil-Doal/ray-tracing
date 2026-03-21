#pragma once

#include <vector>
#include <memory>
#include <string>
#include "../core/camera.h"
#include "../core/hittable.h"
#include "../objects/hittable_list.h"
#include "../objects/bvh_node.h"
#include "../lights/light.h"

// Everything a renderer (CPU or CUDA) needs to render a frame.
// Built once by a scene function, consumed by both render paths.
struct SceneDesc {
  // Geometry
  HittableList world;

  // Lights (explicit light list for NEE / direct lighting)
  std::vector<std::shared_ptr<Light>> lights;

  // Camera parameters
  Vec3  lookfrom   = Vec3(0, 2, 5);
  Vec3  lookat     = Vec3(0, 0, 0);
  Vec3  vup        = Vec3(0, 1, 0);
  double vfov      = 60.0;
  double aperture   = 0.0;
  double focus_dist = -1.0;  // negative => auto-compute from lookfrom-lookat

  // Image dimensions (scene can suggest, main can override)
  int width  = 1080;
  int height = 720;
  int samples_per_pixel = 128;
  int max_depth = 20;

  // Sky / environment
  std::string sky_hdr_path;    // empty => gradient fallback
  float sky_intensity = 1.0f;

  // Build camera from stored params (call after setting width/height)
  Camera build_camera() const {
    double aspect = double(width) / height;
    double fd = focus_dist > 0 ? focus_dist : (lookfrom - lookat).norm();
    return Camera(lookfrom, lookat, vup, vfov, aspect, aperture, fd);
  }

  // Build BVH over world objects (convenience)
  std::shared_ptr<BVHNode> build_bvh() const {
    auto objs = world.objects; // copy because BVHNode sorts in-place
    return std::make_shared<BVHNode>(objs, 0, objs.size());
  }
};