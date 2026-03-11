#define TINYOBJLOADER_IMPLEMENTATION
#include <iostream>
#include <chrono>
#include <vector>
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../objects/hittable_list.h"
#include "../materials/material.h"
#include "../materials/lambertian.h"
#include "../core/camera.h"
#include "../objects/bvh_node.h"
#include "../objects/triangle.h"
#include "../objects/obj_loader.h"
#include "../renderer/renderer.h"

using namespace std::chrono;

int main() {
    int width = 400;
    int height = 200;
    int samples_per_pixel = 10;
    int max_depth = 5;

    std::cout << "Generating world...\n";
    HittableList world;
    Material* white = new Lambertian(Vec3(0.8,0.8,0.8));

    // Load bunny as triangles 
    std::vector<Triangle*> triangles = load_obj_triangle("../assets/models/bunny.obj", white, 10.0, Vec3(0,0,0));
    for(auto t : triangles)
        world.add(t);

    // Camera setup
    double aspect_ratio = double(width)/height;
    Vec3 lookfrom(1,2,1);
    Vec3 lookat(0,0,0);
    Vec3 vup(0,1,0);
    double focus_dist = (lookfrom - lookat).norm();
    double aperture = 0.0;
    Camera camera(lookfrom, lookat, vup, 20, aspect_ratio, aperture, focus_dist);

    std::vector<Vec3> framebuffer(width*height);

    std::cout << "Rendering: raw single-threaded...\n";
    auto start1 = high_resolution_clock::now();
    render_rows(0, height, width, height, samples_per_pixel, max_depth, camera, world, framebuffer);
    auto end1 = high_resolution_clock::now();
    std::cout << "Raw: " << duration_cast<seconds>(end1-start1).count() << " s\n";

    std::cout << "Building BVH...\n";
    BVHNode world_bvh(world.objects, 0, world.objects.size());

    std::cout << "Rendering: BVH single-threaded...\n";
    auto start2 = high_resolution_clock::now();
    render_rows(0, height, width, height, samples_per_pixel, max_depth, camera, world_bvh, framebuffer);
    auto end2 = high_resolution_clock::now();
    std::cout << "BVH: " << duration_cast<seconds>(end2-start2).count() << " s\n";

    std::cout << "Rendering: BVH multithreaded...\n";
    auto start3 = high_resolution_clock::now();
    render_image(width, height, samples_per_pixel, max_depth, camera, world_bvh, framebuffer);
    auto end3 = high_resolution_clock::now();
    std::cout << "BVH + multithreaded: " << duration_cast<seconds>(end3-start3).count() << " s\n";

    return 0;
}