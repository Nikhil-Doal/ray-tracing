#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../objects/sphere.h"
#include "../objects/plane.h"
#include "../objects/hittable_list.h"
#include "../materials/material.h"
#include "../materials/lambertian.h"
#include "../materials/metal.h"
#include "../materials/dielectric.h"
#include "../materials/glossy.h"
#include "../core/camera.h"
#include "../objects/bvh_node.h"
#include "../objects/obj_loader.h"
#include "../renderer/renderer.h"
#include "../utils/image_writer.h"
#include "../textures/solid_color.h"
#include "../textures/image_texture.h"
#include "../textures/hdr_texture.h"
#include "../lights/point_light.h"
#include "../lights/directional_light.h"
#include "../objects/rotate.h"
#include "../materials/diffuse_light.h"
#include "../lights/area_light.h"
#include "../objects/triangle.h"
#include <memory>

// helper: build a quad from two triangles with given normal and UVs
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

// helper: build a solid box (6 faces) from min/max corners
static void add_box(HittableList &world,
  Vec3 mn, Vec3 mx, std::shared_ptr<Material> mat)
{
  float w = mx.x - mn.x;
  float h = mx.y - mn.y;
  float d = mx.z - mn.z;
  Vec3 uv00(0,0,0), uv10(1,0,0), uv11(1,1,0), uv01(0,1,0);

  // front (+Z)
  add_quad(world,
    Vec3(mn.x,mn.y,mx.z), Vec3(mx.x,mn.y,mx.z),
    Vec3(mx.x,mx.y,mx.z), Vec3(mn.x,mx.y,mx.z),
    mat, uv00, uv10, uv11, uv01, Vec3(0,0,1));
  // back (-Z)
  add_quad(world,
    Vec3(mx.x,mn.y,mn.z), Vec3(mn.x,mn.y,mn.z),
    Vec3(mn.x,mx.y,mn.z), Vec3(mx.x,mx.y,mn.z),
    mat, uv00, uv10, uv11, uv01, Vec3(0,0,-1));
  // left (-X)
  add_quad(world,
    Vec3(mn.x,mn.y,mn.z), Vec3(mn.x,mn.y,mx.z),
    Vec3(mn.x,mx.y,mx.z), Vec3(mn.x,mx.y,mn.z),
    mat, uv00, uv10, uv11, uv01, Vec3(-1,0,0));
  // right (+X)
  add_quad(world,
    Vec3(mx.x,mn.y,mx.z), Vec3(mx.x,mn.y,mn.z),
    Vec3(mx.x,mx.y,mn.z), Vec3(mx.x,mx.y,mx.z),
    mat, uv00, uv10, uv11, uv01, Vec3(1,0,0));
  // top (+Y)
  add_quad(world,
    Vec3(mn.x,mx.y,mx.z), Vec3(mx.x,mx.y,mx.z),
    Vec3(mx.x,mx.y,mn.z), Vec3(mn.x,mx.y,mn.z),
    mat, uv00, uv10, uv11, uv01, Vec3(0,1,0));
  // bottom (-Y)
  add_quad(world,
    Vec3(mn.x,mn.y,mn.z), Vec3(mx.x,mn.y,mn.z),
    Vec3(mx.x,mn.y,mx.z), Vec3(mn.x,mn.y,mx.z),
    mat, uv00, uv10, uv11, uv01, Vec3(0,-1,0));
}

int main() {
  // ================================================================
  // Settings — lower samples while iterating, bump up for final
  // ================================================================
  int width = 1280;
  int height = 720;
  const int samples_per_pixel = 64;
  const int max_depth = 16;
  const double exposure = 1.4;

  HittableList world;
  LightList lights;

  // ================================================================
  // HDR sky — visible through the window openings
  // ================================================================
  g_sky_texture = std::make_shared<HdrTexture>("../../assets/sky.hdr");
  g_sky_intensity = 2.0f;

  // ================================================================
  // Materials
  // ================================================================
  // stone walls — slightly rough, barely any specular
  auto stone_tex = std::make_shared<ImageTexture>("../../assets/bricks.jpg");
  auto stone_mat = std::make_shared<Glossy>(stone_tex, 0.8, 0.04);
  stone_mat->set_normal_map(
    std::make_shared<ImageTexture>("../../assets/bricks_normal.jpg", true), 2.0);
  stone_mat->set_bump_map(
    std::make_shared<ImageTexture>("../../assets/bricks_bump.jpg", true), 3.0);

  // floor — dark polished stone
  auto floor_mat = std::make_shared<Glossy>(
    std::make_shared<SolidColor>(Vec3(0.08, 0.07, 0.06)), 0.4, 0.12);

  // ceiling — same stone
  auto ceil_mat = std::make_shared<Lambertian>(
    std::make_shared<SolidColor>(Vec3(0.12, 0.11, 0.10)));

  // pillar — slightly lighter stone
  auto pillar_mat = std::make_shared<Glossy>(
    std::make_shared<SolidColor>(Vec3(0.18, 0.17, 0.15)), 0.6, 0.06);

  // ================================================================
  // Cathedral dimensions
  // ================================================================
  // Long hall stretching along Z axis
  // Camera will be inside looking toward the far end + upward
  double room_w  = 16.0;  // total width (X: -8 to +8)
  double room_h  = 14.0;  // ceiling height
  double room_d  = 40.0;  // depth along Z (-20 to +20)
  double half_w  = room_w / 2.0;
  double half_d  = room_d / 2.0;

  float tw = 4.0f; // wall UV tiling
  float tf = 6.0f; // floor UV tiling

  // ================================================================
  // Floor
  // ================================================================
  add_quad(world,
    Vec3(-half_w, 0, -half_d), Vec3(half_w, 0, -half_d),
    Vec3(half_w, 0, half_d),   Vec3(-half_w, 0, half_d),
    floor_mat,
    Vec3(0,0,0), Vec3(tf,0,0), Vec3(tf,tf,0), Vec3(0,tf,0),
    Vec3(0, 1, 0));

  // ================================================================
  // Ceiling — solid slab with window openings cut out
  // We build the ceiling as several panels, leaving gaps for windows
  // ================================================================
  // Window openings: 3 rectangular skylights along the center of the ceiling
  // Each window is 3 units wide (X: -1.5 to +1.5), 4 units long (Z),
  // spaced along Z.
  //
  // Ceiling panels fill around the windows:
  //   - full width strips at the ends
  //   - side panels flanking each window
  //
  // Window positions along Z: centered at z=-10, z=0, z=+10
  double win_half_w = 1.5;
  double win_half_d = 2.0;
  double cy = room_h; // ceiling Y

  // helper for ceiling panels
  auto add_ceil = [&](double x0, double z0, double x1, double z1) {
    add_quad(world,
      Vec3(x0, cy, z0), Vec3(x1, cy, z0),
      Vec3(x1, cy, z1), Vec3(x0, cy, z1),
      ceil_mat,
      Vec3(0,0,0), Vec3(1,0,0), Vec3(1,1,0), Vec3(0,1,0),
      Vec3(0, -1, 0));
  };

  // full ceiling strip: z = -20 to -12 (before first window)
  add_ceil(-half_w, -half_d, half_w, -12);
  // between window 1 and 2: z = -8 to -2
  add_ceil(-half_w, -8, half_w, -2);
  // between window 2 and 3: z = +2 to +8
  add_ceil(-half_w, 2, half_w, 8);
  // after last window: z = +12 to +20
  add_ceil(-half_w, 12, half_w, half_d);

  // side panels flanking each window (left and right of each opening)
  double win_centers[] = {-10.0, 0.0, 10.0};
  for (double wz : win_centers) {
    // left side panel
    add_ceil(-half_w, wz - win_half_d, -win_half_w, wz + win_half_d);
    // right side panel
    add_ceil(win_half_w, wz - win_half_d, half_w, wz + win_half_d);
  }

  // ================================================================
  // Walls — left and right
  // ================================================================
  // Left wall (-X)
  add_quad(world,
    Vec3(-half_w, 0, -half_d), Vec3(-half_w, 0, half_d),
    Vec3(-half_w, room_h, half_d), Vec3(-half_w, room_h, -half_d),
    stone_mat,
    Vec3(0,0,0), Vec3(tw,0,0), Vec3(tw,tw*0.5f,0), Vec3(0,tw*0.5f,0),
    Vec3(1, 0, 0));

  // Right wall (+X)
  add_quad(world,
    Vec3(half_w, 0, half_d), Vec3(half_w, 0, -half_d),
    Vec3(half_w, room_h, -half_d), Vec3(half_w, room_h, half_d),
    stone_mat,
    Vec3(0,0,0), Vec3(tw,0,0), Vec3(tw,tw*0.5f,0), Vec3(0,tw*0.5f,0),
    Vec3(-1, 0, 0));

  // Back wall (-Z)
  add_quad(world,
    Vec3(half_w, 0, -half_d), Vec3(-half_w, 0, -half_d),
    Vec3(-half_w, room_h, -half_d), Vec3(half_w, room_h, -half_d),
    stone_mat,
    Vec3(0,0,0), Vec3(tw,0,0), Vec3(tw,tw*0.5f,0), Vec3(0,tw*0.5f,0),
    Vec3(0, 0, 1));

  // Front wall (+Z) — we omit this so the camera entrance is open,
  // or add it behind the camera. Let's add it for enclosure.
  add_quad(world,
    Vec3(-half_w, 0, half_d), Vec3(half_w, 0, half_d),
    Vec3(half_w, room_h, half_d), Vec3(-half_w, room_h, half_d),
    stone_mat,
    Vec3(0,0,0), Vec3(tw,0,0), Vec3(tw,tw*0.5f,0), Vec3(0,tw*0.5f,0),
    Vec3(0, 0, -1));

  // ================================================================
  // Pillars — two rows along the hall, creating shadow occluders
  // These break up the light from the skylights into visible shafts
  // ================================================================
  double pillar_w = 0.8;
  double pillar_h = room_h;
  double pillar_x_left  = -4.0;
  double pillar_x_right =  4.0;
  double pillar_z_positions[] = {-14.0, -7.0, 0.0, 7.0, 14.0};

  for (double pz : pillar_z_positions) {
    // left row
    add_box(world,
      Vec3(pillar_x_left - pillar_w/2, 0, pz - pillar_w/2),
      Vec3(pillar_x_left + pillar_w/2, pillar_h, pz + pillar_w/2),
      pillar_mat);
    // right row
    add_box(world,
      Vec3(pillar_x_right - pillar_w/2, 0, pz - pillar_w/2),
      Vec3(pillar_x_right + pillar_w/2, pillar_h, pz + pillar_w/2),
      pillar_mat);
  }

  // ================================================================
  // Light sources — strong emitters ABOVE the skylight openings
  // These sit just above the ceiling so light pours down through the gaps
  // ================================================================
  for (double wz : win_centers) {
    Vec3 lpos(0, cy + 3.0, wz);
    double lrad = 1.5;
    Vec3 lcol(1.0, 0.92, 0.8); // warm sunlight
    double lint = 25.0;         // strong so shafts are bright

    auto lmat = std::make_shared<DiffuseLight>(lcol * lint);
    world.add(std::make_shared<Sphere>(lpos, lrad, lmat));
    lights.push_back(std::make_shared<SphereAreaLight>(
      lpos, lrad, lcol, lint));
  }

  // ================================================================
  // A few objects on the floor for visual interest
  // ================================================================
  // Glass orb catching light shafts
  auto glass = std::make_shared<Dielectric>(1.5);
  world.add(std::make_shared<Sphere>(Vec3(0, 1.0, -8), 1.0, glass));

  // Small golden sphere
  auto gold = std::make_shared<Metal>(
    std::make_shared<SolidColor>(Vec3(0.85, 0.65, 0.2)), 0.15);
  world.add(std::make_shared<Sphere>(Vec3(-2.5, 0.6, -3), 0.6, gold));

  // Matte red sphere
  auto red = std::make_shared<Lambertian>(
    std::make_shared<SolidColor>(Vec3(0.7, 0.1, 0.1)));
  world.add(std::make_shared<Sphere>(Vec3(2.5, 0.5, 2), 0.5, red));

  // ================================================================
  // Volumetric medium — the dusty cathedral air
  // ================================================================
  // Key insight: god rays need:
  //   1. camera looking TOWARD or NEAR the light (forward scattering)
  //   2. occluders (pillars, ceiling) casting shadows INTO the medium
  //   3. high enough sigma_s that the in-scatter is visible
  //
  // sigma_s=0.06 is thick enough to see shafts in a room this size.
  // g=0.75 concentrates scattering in the forward direction (toward camera
  // when camera faces the skylights).
  auto vol = std::make_shared<VolumeRegion>();
  vol->bounds = VolumeAABB{
    Vec3(-half_w, 0, -half_d),
    Vec3(half_w, room_h, half_d)};
  vol->sigma_s = 0.06;   // scattering density — dusty air
  vol->sigma_a = 0.004;  // slight warm absorption
  vol->g       = 0.75;   // strong forward scattering
  g_volume = vol;
  g_volume_steps = 32;   // 32 for speed, 64 for cleaner result

  // ================================================================
  // Build BVH
  // ================================================================
  std::cout << "Building BVH over " << world.objects.size() << " objects...\n";
  BVHNode world_bvh(world.objects, 0, world.objects.size());

  // ================================================================
  // Camera — inside the hall, looking DOWN the nave toward skylights
  // Slightly upward angle so we see the ceiling gaps and light shafts
  // ================================================================
  double aspect = double(width) / height;
  Vec3 lookfrom(0, 4.0, 16);    // near the +Z end, eye height
  Vec3 lookat(0, 6.0, -5);      // looking forward and slightly up
  double focus_dist = (lookfrom - lookat).norm();
  Camera camera(lookfrom, lookat, Vec3(0,1,0), 70, aspect, 0.0, focus_dist);

  // ================================================================
  // Render
  // ================================================================
  std::cout << "Rendering cathedral scene (CPU)...\n";
  std::cout << "  " << width << "x" << height
            << " @ " << samples_per_pixel << " spp\n";
  std::cout << "  Sky intensity: " << g_sky_intensity << "\n";
  std::cout << "  Volume: sigma_s=" << vol->sigma_s
            << " sigma_a=" << vol->sigma_a
            << " g=" << vol->g
            << " steps=" << g_volume_steps << "\n";
  std::cout << "  Exposure: " << exposure << "\n";

  std::vector<Vec3> framebuffer(width * height);
  render_image(width, height, samples_per_pixel, max_depth,
               camera, world_bvh, lights, framebuffer);

  save_png("../../image.png", width, height, framebuffer,
           samples_per_pixel, exposure);

  std::cout << "Done — saved image.png\n";
  return 0;
}