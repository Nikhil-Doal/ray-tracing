// src/benchmark/test.cu
//
// Runs all four render paths on the same scene with identical settings:
//   [1] Raw CPU — HittableList linear scan, single thread
//   [2] BVH CPU — SAH BVH, single thread
//   [3] BVH MT — SAH BVH, all hardware threads, tile work-stealing
//   [4] CUDA — GPU path
//
// Usage (from build/Release/):
//   ./benchmark test
//   ./benchmark sponza
//   ./benchmark random_spheres

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <vector>
#include <string>
#include <thread>

#include "../../core/vec3.h"
#include "../../core/camera.h"
#include "../../objects/hittable_list.h"
#include "../../objects/bvh_node.h"
#include "../../renderer/renderer.h"
#include "../../textures/hdr_texture.h"
#include "../../scenes/scene_registry.h"
#include "../../renderer/cuda/cuda_renderer.cuh"
#include "../../renderer/cuda/cuda_bvh.cuh"
#include "../../lights/directional_light.h"
#include "../../lights/area_light.h"
#include "../../external/stbimagewrite/stb_image.h"

using Clock = std::chrono::high_resolution_clock;
using Sec   = std::chrono::duration<double>;

// Helpers
static void hr(char c = '-', int n = 70) { std::cout << std::string(n, c) << '\n'; }

static std::string fmt_time(double s) {
    std::ostringstream ss;
    if (s < 60.0) ss << std::fixed << std::setprecision(3) << s << "s";
    else if (s < 3600.0) ss << (int)(s/60) << "m " << std::fixed << std::setprecision(1) << fmod(s,60.0) << "s";
    else ss << (int)(s/3600) << "h " << (int)(fmod(s,3600.0)/60) << "m";
    return ss.str();
}

static std::vector<GpuLight> convert_lights(const std::vector<std::shared_ptr<Light>> &lights) {
    std::vector<GpuLight> out;
    for (const auto &l : lights) {
        GpuLight g{};
        if (auto *d = dynamic_cast<DirectionalLight*>(l.get())) {
            g.type = GpuLightType::DIRECTIONAL;
            g.direction = {(float)d->direction.x, (float)d->direction.y, (float)d->direction.z};
            g.color = {(float)d->color.x, (float)d->color.y, (float)d->color.z};
            g.intensity = (float)d->intensity;
        } else if (auto *s = dynamic_cast<SphereAreaLight*>(l.get())) {
            g.type = GpuLightType::SPHERE_AREA;
            g.position  = {(float)s->center.x, (float)s->center.y, (float)s->center.z};
            g.radius = (float)s->radius; 
            g.color = {(float)s->color.x,  (float)s->color.y,  (float)s->color.z};
            g.intensity = (float)s->intensity;
        }
        out.push_back(g);
    }
    return out;
}

static GpuCamera convert_camera(const Camera &c) {
    GpuCamera gc{};
    gc.origin = {(float)c.origin.x, (float)c.origin.y, (float)c.origin.z};
    gc.lower_left_corner = {(float)c.lower_left_corner.x, (float)c.lower_left_corner.y, (float)c.lower_left_corner.z};
    gc.horizontal = {(float)c.horizontal.x, (float)c.horizontal.y, (float)c.horizontal.z};
    gc.vertical = {(float)c.vertical.x, (float)c.vertical.y, (float)c.vertical.z};
    gc.lens_radius = (float)c.lens_radius;
    return gc;
}

struct Result {
    std::string label;
    double render_s  = 0.0;
    double setup_s   = 0.0;   // BVH build (CPU) or scene flatten (CUDA)
    std::string setup_label;
    double mpx_spp_s = 0.0;   // Mega pixel-samples per second
};

// raw cpu - directly use hittable list
static Result bench_raw(const SceneDesc &d, const Camera &cam) {
    std::vector<Vec3> fb(d.width * d.height, Vec3(0,0,0));

    auto t0 = Clock::now();
    render_rows(0, d.height, d.width, d.height, d.samples_per_pixel, d.max_depth, cam, d.world, d.lights, fb);
    double elapsed = Sec(Clock::now() - t0).count();

    return {"Raw CPU  (no BVH, 1 thread)", elapsed, 0.0, "", (double)d.width * d.height * d.samples_per_pixel / (elapsed * 1e6) };
}

// raw cpu - with bvh (single thread)
static Result bench_bvh_single(const SceneDesc &d, const Camera &cam) {
    auto t0  = Clock::now();
    auto bvh = d.build_bvh();
    double setup = Sec(Clock::now() - t0).count();

    std::vector<Vec3> fb(d.width * d.height, Vec3(0,0,0));
    auto t1 = Clock::now();
    render_rows(0, d.height, d.width, d.height, d.samples_per_pixel, d.max_depth, cam, *bvh, d.lights, fb);
    double elapsed = Sec(Clock::now() - t1).count();

    return {"BVH CPU  (BVH, 1 thread)", elapsed, setup, "BVH build", (double)d.width * d.height * d.samples_per_pixel / (elapsed * 1e6)};
}

// multithreading + work stealing with bvh 
static Result bench_bvh_mt(const SceneDesc &d, const Camera &cam) {
    unsigned int nt = std::thread::hardware_concurrency();

    auto t0  = Clock::now();
    auto bvh = d.build_bvh();
    double setup = Sec(Clock::now() - t0).count();

    std::vector<Vec3> fb(d.width * d.height, Vec3(0,0,0));
    auto t1 = Clock::now();
    render_image(d.width, d.height, d.samples_per_pixel, d.max_depth, cam, *bvh, d.lights, fb);
    double elapsed = Sec(Clock::now() - t1).count();

    return { "BVH MT   (BVH, " + std::to_string(nt) + " threads)", elapsed, setup, "BVH build", (double)d.width * d.height * d.samples_per_pixel / (elapsed * 1e6) };
}

// CUDA accelerated 
static Result bench_cuda(const SceneDesc &d, const Camera &cam) {
    // Flatten scene + build GPU BVH (CPU side, separate from GPU time)
    auto t0 = Clock::now();
    FlatScene flat = build_flat_scene(d.world);
    double setup = Sec(Clock::now() - t0).count();

    auto gpu_lights = convert_lights(d.lights);

    GpuScene host{};
    host.primitives = flat.primitives.data();
    host.num_primitives = (int)flat.primitives.size();
    host.bvh_nodes = flat.bvh_nodes.data();
    host.num_bvh_nodes  = (int)flat.bvh_nodes.size();
    host.bvh_root = flat.root;
    host.materials = flat.materials.data();
    host.num_materials  = (int)flat.materials.size();
    host.tex_data = flat.tex_data.empty() ? nullptr : flat.tex_data.data();
    host.textures = flat.textures.empty() ? nullptr : flat.textures.data();
    host.num_textures = (int)flat.textures.size();
    host.lights = nullptr;
    host.num_lights = (int)gpu_lights.size();

    // HDR sky
    host.sky = {};
    float *sky_data = nullptr;
    int sky_w = 0, sky_h = 0, sky_c = 0;
    if (!d.sky_hdr_path.empty()) {
        sky_data = stbi_loadf(d.sky_hdr_path.c_str(), &sky_w, &sky_h, &sky_c, 3);
        if (sky_data) {
            host.sky.enabled   = true;
            host.sky.data      = sky_data;
            host.sky.width     = sky_w;
            host.sky.height    = sky_h;
            host.sky.intensity = d.sky_intensity;
        }
    }

    CudaRenderParams params{};
    params.width = d.width;
    params.height = d.height;
    params.samples_per_pixel = d.samples_per_pixel;
    params.max_depth = d.max_depth;
    params.camera = convert_camera(cam);

    std::vector<float> fb;

    // Empty string → cuda_render skips all progressive PNG saves
    auto t1 = Clock::now();
    cuda_render(params, host, gpu_lights, fb, "");
    double elapsed = Sec(Clock::now() - t1).count();

    if (sky_data) stbi_image_free(sky_data);

    return { "CUDA GPU (BVH, all CUDA cores)", elapsed, setup, "Flatten+BVH build", (double)d.width * d.height * d.samples_per_pixel / (elapsed * 1e6) };
}

// print results
static void print_results(const std::string &scene_name, const SceneDesc &d, const std::vector<Result> &results) {
    std::cout << "\n\n"; hr('='); std::cout << "  RESULTS — " << scene_name << "\n"; hr('=');
    std::cout << "  " << d.width << "x" << d.height << "  @  " << d.samples_per_pixel << " spp  depth " << d.max_depth << "  primitives " << d.world.objects.size() << "\n\n";

    std::cout << std::left << std::setw(36) << "  Path" << std::right << std::setw(12) << "Render" << std::setw(16) << "Setup" << std::setw(14) << "Mpx-spp/s" << std::setw(10) << "Speedup" << "\n";
    hr();

    double base = -1.0;
    for (const auto &r : results) {
        std::cout << std::left << std::setw(36) << ("  " + r.label);
        std::cout << std::right << std::setw(12) << fmt_time(r.render_s);

        if (r.setup_s > 0.0) { std::string s = r.setup_label + " " + fmt_time(r.setup_s); std::cout << std::setw(16) << s; }
        else { std::cout << std::setw(16) << "-"; }

        std::cout << std::fixed << std::setprecision(3) << std::setw(14) << r.mpx_spp_s;

        if (base < 0.0) { base = r.mpx_spp_s; std::cout << std::setw(10) << "1.00x"; }
        else { std::ostringstream ss; ss << std::fixed << std::setprecision(2) << (r.mpx_spp_s / base) << "x"; std::cout << std::setw(10) << ss.str(); }

        std::cout << "\n";
    }

    hr();
    std::cout << "  Render  = pure render time (excludes Setup)\n  Setup   = BVH build (CPU) or scene flatten + GPU BVH (CUDA)\n  Speedup = throughput ratio vs Raw CPU\n";
    hr('=');
    std::cout << "\n";
}

// main
int main(int argc, char *argv[]) {
    const std::string scene_name = (argc > 1) ? argv[1] : "test";

    hr('='); std::cout << "  RAYTRACER BENCHMARK\n"; hr('=');
    std::cout << "  Scene : " << scene_name << "\n";
    std::cout << "  Cores : " << std::thread::hardware_concurrency() << "\n\n";

    SceneDesc d = SceneRegistry::build(scene_name);
    if (d.world.objects.empty()) {
        std::cerr << "  [ERROR] Scene \"" << scene_name << "\" not found or empty.\n";
        std::cerr << "  Available:"; for (auto &s : SceneRegistry::list()) std::cerr << "  " << s;
        std::cerr << "\n"; return 1;
    }

    std::cout << "  Primitives : " << d.world.objects.size() << "\n";
    std::cout << "  Config     : " << d.width << "x" << d.height << " @ " << d.samples_per_pixel << " spp, depth " << d.max_depth << "\n";
    std::cout << "\n  NOTE: All four paths run at these exact settings.\n        Raw CPU with no BVH on large scenes may take a very long time.\n\n";

    if (!d.sky_hdr_path.empty()) {
        auto hdr = std::make_shared<HdrTexture>(d.sky_hdr_path);
        if (hdr->data) { g_sky_texture = hdr; g_sky_intensity = d.sky_intensity; }
    }

    const Camera cam = d.build_camera();
    std::vector<Result> results; results.reserve(4);

    // hr(); std::cout << "  [1/4] Raw CPU (no BVH, single-threaded)...\n"; std::cout.flush();
    // results.push_back(bench_raw(d, cam));
    // std::cout << "        " << fmt_time(results.back().render_s) << "\n";

    hr(); std::cout << "  [2/4] BVH CPU (single-threaded)...\n"; std::cout.flush();
    results.push_back(bench_bvh_single(d, cam));
    std::cout << "        render " << fmt_time(results.back().render_s) << "  bvh build " << fmt_time(results.back().setup_s) << "\n";

    hr(); std::cout << "  [3/4] BVH CPU (" << std::thread::hardware_concurrency() << " threads, tiled)...\n"; std::cout.flush();
    results.push_back(bench_bvh_mt(d, cam));
    std::cout << "        render " << fmt_time(results.back().render_s) << "  bvh build " << fmt_time(results.back().setup_s) << "\n";

    hr(); std::cout << "  [4/4] CUDA GPU...\n"; std::cout.flush();
    results.push_back(bench_cuda(d, cam));
    std::cout << "        render " << fmt_time(results.back().render_s) << "  flatten+bvh " << fmt_time(results.back().setup_s) << "\n";

    print_results(scene_name, d, results);
    return 0;
}
