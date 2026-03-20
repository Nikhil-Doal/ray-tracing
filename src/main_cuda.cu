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
    int samples_per_pixel = 128;
    int max_depth = 20;

    HittableList world;
    LightList lights;

    float tile = 1.0f;

    auto nmap_mat = std::make_shared<Glossy>(std::make_shared<ImageTexture>("../../assets/bricks.jpg"), 0.3, 0.5);
    nmap_mat->set_normal_map(std::make_shared<ImageTexture>("../../assets/bricks_normal.jpg", true), 5.0);
    nmap_mat->set_bump_map(std::make_shared<ImageTexture>("../../assets/bricks_bump.jpg", true), 10.0);

    // Left floor
    world.add(std::make_shared<Triangle>(Vec3(-15,0,-15), Vec3(0,0,15), Vec3(0,0,-15), nmap_mat,
        Vec3(0,0,0), Vec3(tile,tile,0), Vec3(tile,0,0), Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
    world.add(std::make_shared<Triangle>(Vec3(-15,0,-15), Vec3(-15,0,15), Vec3(0,0,15), nmap_mat,
        Vec3(0,0,0), Vec3(0,tile,0), Vec3(tile,tile,0), Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
    // Left wall
    world.add(std::make_shared<Triangle>(Vec3(-15,0,-5), Vec3(0,0,-5), Vec3(0,8,-5), nmap_mat,
        Vec3(0,0,0), Vec3(tile,0,0), Vec3(tile,tile*0.5f,0), Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));
    world.add(std::make_shared<Triangle>(Vec3(-15,0,-5), Vec3(0,8,-5), Vec3(-15,8,-5), nmap_mat,
        Vec3(0,0,0), Vec3(tile,tile*0.5f,0), Vec3(0,tile*0.5f,0), Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));

    auto flat_mat = std::make_shared<Glossy>(std::make_shared<ImageTexture>("../../assets/bricks.jpg"), 0.3, 0.5);
    // Right floor
    world.add(std::make_shared<Triangle>(Vec3(0,0,-15), Vec3(15,0,15), Vec3(15,0,-15), flat_mat,
        Vec3(0,0,0), Vec3(tile,tile,0), Vec3(tile,0,0), Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
    world.add(std::make_shared<Triangle>(Vec3(0,0,-15), Vec3(0,0,15), Vec3(15,0,15), flat_mat,
        Vec3(0,0,0), Vec3(0,tile,0), Vec3(tile,tile,0), Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0)));
    // Right wall
    world.add(std::make_shared<Triangle>(Vec3(0,0,-5), Vec3(15,0,-5), Vec3(15,8,-5), flat_mat,
        Vec3(0,0,0), Vec3(tile,0,0), Vec3(tile,tile*0.5f,0), Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));
    world.add(std::make_shared<Triangle>(Vec3(0,0,-5), Vec3(15,8,-5), Vec3(0,8,-5), flat_mat,
        Vec3(0,0,0), Vec3(tile,tile*0.5f,0), Vec3(0,tile*0.5f,0), Vec3(0,0,1), Vec3(0,0,1), Vec3(0,0,1)));

    // Area light
    auto light_mat = std::make_shared<DiffuseLight>(Vec3(1.0, 0.95, 0.8) * 8.0);
    world.add(std::make_shared<Sphere>(Vec3(0, 20, 0), 3.0, light_mat));
    lights.push_back(std::make_shared<SphereAreaLight>(Vec3(0, 20, 0), 3.0, Vec3(1.0, 0.95, 0.8), 8.0));

    BVHNode world_bvh(world.objects, 0, world.objects.size());

    double aspect = double(width) / height;
    Vec3 lookfrom(0, 6, 14);
    Vec3 lookat(0, 2, -2);
    Camera camera(lookfrom, lookat, Vec3(0,1,0), 65, aspect, 0.0, (lookfrom-lookat).norm());

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

    // HDR sky — FIX: intensity 3.0 instead of 1.0 to compensate for linear-space dimness
    host_scene.sky.enabled   = false;
    host_scene.sky.data      = nullptr;
    host_scene.sky.width     = 0;
    host_scene.sky.height    = 0;
    host_scene.sky.intensity = 3.0f;  // was 1.0

    int sky_w, sky_h, sky_c;
    float *sky_data = stbi_loadf("../../assets/sky.hdr", &sky_w, &sky_h, &sky_c, 3);
    if (sky_data) {
        host_scene.sky.enabled = true;
        host_scene.sky.data    = sky_data;
        host_scene.sky.width   = sky_w;
        host_scene.sky.height  = sky_h;
        std::cout << "HDR sky loaded: " << sky_w << "x" << sky_h << " (intensity=" << host_scene.sky.intensity << "x)\n";
    } else {
        std::cerr << "Warning: Could not load sky.hdr — using gradient fallback\n";
    }

    // ---- God rays: configure the volumetric medium ----
    host_scene.volume.enabled  = true;
    host_scene.volume.aabb_min = {-16.0f, 0.0f, -16.0f};
    host_scene.volume.aabb_max = {16.0f, 22.0f, 16.0f};
    host_scene.volume.sigma_s  = 0.04f;   // scattering strength
    host_scene.volume.sigma_a  = 0.005f;  // absorption
    host_scene.volume.g        = 0.6f;    // forward scattering for shaft-like god rays

    CudaRenderParams params{};
    params.width             = width;
    params.height            = height;
    params.samples_per_pixel = samples_per_pixel;
    params.max_depth         = max_depth;
    params.camera            = convert_camera(camera);

    std::cout << "Rendering on GPU...\n";
    std::cout << "  God rays: sigma_s=" << host_scene.volume.sigma_s
              << " sigma_a=" << host_scene.volume.sigma_a
              << " g=" << host_scene.volume.g << "\n";

    std::vector<float> fb;
    cuda_render(params, host_scene, gpu_lights, fb);

    if (sky_data) stbi_image_free(sky_data);

    std::vector<Vec3> framebuffer(width * height);
    for (int i = 0; i < width * height; ++i)
        framebuffer[i] = Vec3(fb[i*3+0], fb[i*3+1], fb[i*3+2]);

    save_png("../../image_cuda.png", width, height, framebuffer, 1);
    std::cout << "Saved image_cuda.png\n";
    return 0;
}
