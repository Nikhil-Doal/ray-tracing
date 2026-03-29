// scene_test.cpp

#include "scene_registry.h"
#include "../objects/triangle.h"
#include "../objects/obj_loader.h"
#include "../materials/lambertian.h"
#include "../textures/solid_color.h"
#include "../lights/directional_light.h"

static SceneDesc build_test() {
    SceneDesc scene;
    scene.width             = 1080;
    scene.height            = 720;
    scene.samples_per_pixel = 100;
    scene.max_depth         = 16;

    // 3/4 view, tilted down slightly so the full body is visible
    scene.lookfrom = Vec3(3.5,  4.5, 9.5);
    scene.lookat = Vec3(0.0,  2.2, 0.0);
    scene.vfov = 35.0;
    scene.aperture = 0.0;

    scene.sky_hdr_path = "../../assets/sky.hdr";
    scene.sky_intensity = 0.8f;

    auto mat = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.80, 0.80, 0.80)));
    auto tris = load_obj_triangle("../../assets/models/bunny.obj", mat, 30.0, Vec3(0.0, -1.0, 0.0));
    for (auto &t : tris) scene.world.add(t);

    auto floor = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.45, 0.45, 0.45)));
    scene.world.add(std::make_shared<Triangle>(Vec3(-8, 0, -8), Vec3( 8, 0,  8), Vec3( 8, 0, -8), floor, Vec3(0, 0, 0), Vec3(4, 4, 0), Vec3(4, 0, 0), Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 1, 0)));
    scene.world.add(std::make_shared<Triangle>(Vec3(-8, 0, -8), Vec3(-8, 0,  8), Vec3(8, 0, 8), floor, Vec3(0, 0, 0), Vec3(0, 4, 0), Vec3(4, 4, 0), Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 1, 0)));

    scene.lights.push_back(std::make_shared<DirectionalLight>(Vec3(0.3, -2.0, -0.5), Vec3(1.0, 0.93, 0.82), 2.0));

    return scene;
}

REGISTER_SCENE("test", build_test);