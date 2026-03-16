#include "cuda_renderer.cuh"
#include "cuda_scene.cuh"
#include <curand_kernel.h>
#include <cuda_runtime.h>
#include <cstdio>
#include <cmath>

// macro for error check
#define CUDA_CHECK(x) do { \
  cudaError_t e = (x); \
  if (e != cudaSuccess) { \
    printf("CUDA error: %s at %s:%d\n", cudaGetErrorString(e), __FILE__, __LINE__); \
    exit(1); \
  } \
} while (0)

// random number generator helpers
// inline to reduce overhead 
__device__ inline float rand_f(curandState *s) { return curand_uniform(s); }
__device__ inline GpuVec3 rand_in_unit_sphere(curandState *s) {
  float u = curand_uniform(s);
  GpuVec3 p(rand_f(s)*2 - 1, rand_f(s)*2 -1, rand_f(s)*2 -1);
  return p.normalize() * cbrtf(u);
}
__device__ inline GpuVec3 rand_cosine_direction(curandState *s) {
  float r1 = rand_f(s);
  float r2 = rand_f(s);
  float phi = 2.0f * 3.14159265f * r1;
  float x = cosf(phi) * sqrtf(r2);
  float y = sinf(phi) * sqrtf(r2);
  float z = sqrtf(1.0f - r2);
  return {x, y, z};
}

// AABB hit test
__device__ bool aabb_hit(const GpuBVHNode &node, const GpuRay &ray, float t_min, float t_max) {
  for (int i = 0; i < 3; ++i) {
    float inv = 1.0f / ray.direction[i];
    float t0 = (node.aabb_min[i] - ray.origin[i]) * inv;
    float t1 = (node.aabb_max[i] - ray.origin[i]) * inv;
    if (inv < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
    t_min = t0 > t_min ? t0 : t_min;
    t_max = t1 < t_max ? t1 : t_max;
    if (t_max <= t_min) return false;
  }
  return true;
}

// hit sphere
__device__ bool sphere_hit(const GpuSphere &s, const GpuRay &ray, float t_min, float t_max, GpuHitRecord &rec) {
  GpuVec3 oc = ray.origin - s.center;
  float a = ray.direction.dot(ray.direction);
  float b = oc.dot(ray.direction);
  float c = oc.dot(oc) - s.radius * s.radius;
  float disc = b*b - a*c;
  if (disc < 0) return false;

  float t = (-b - sqrtf(disc)) / a;
  if (t < t_min || t > t_max) return false;

  rec.t     = t;
  rec.point = ray.at(t);
  GpuVec3 outward = (rec.point - s.center) * (1.0f / s.radius);
  rec.front_face = ray.direction.dot(outward) < 0;
  rec.normal = rec.front_face ? outward : GpuVec3{-outward.x, -outward.y, -outward.z};

  // spherical UV coords
  float theta = acosf(-outward.y);
  float phi   = atan2f(-outward.z, outward.x) + 3.14159265f;
  rec.u = phi  / (2.0f * 3.14159265f);
  rec.v = theta / 3.14159265f;
  return true;
}

// hit triangle -> moller trumbore
__device__ bool triangle_hit(const GpuTriangle &tri, const GpuRay &ray, float t_min, float t_max, GpuHitRecord &rec) {
  const float EPS = 1e-8f;
  GpuVec3 e1 = tri.v1 - tri.v0;
  GpuVec3 e2 = tri.v2 - tri.v0;
  GpuVec3 h  = ray.direction.cross(e2);
  float   a  = e1.dot(h);
  if (fabsf(a) < EPS) return false;

  float   f  = 1.0f / a;
  GpuVec3 s  = ray.origin - tri.v0;
  float   bu = f * s.dot(h);
  if (bu < 0.0f || bu > 1.0f) return false;

  GpuVec3 q  = s.cross(e1);
  float   bv = f * ray.direction.dot(q);
  if (bv < 0.0f || bu + bv > 1.0f) return false;

  float t = f * e2.dot(q);
  if (t < t_min || t > t_max) return false;

  float bw = 1.0f - bu - bv;
  rec.t     = t;
  rec.point = ray.at(t);
  rec.u     = bw*tri.uv0.x + bu*tri.uv1.x + bv*tri.uv2.x;
  rec.v     = bw*tri.uv0.y + bu*tri.uv1.y + bv*tri.uv2.y;

  GpuVec3 n = e1.cross(e2).normalize();
  rec.front_face = ray.direction.dot(n) < 0;
  rec.normal     = rec.front_face ? n : GpuVec3{-n.x, -n.y, -n.z};
  return true;
}

// BVH traversal -> stack based to prevent recursion limit on gpu
__device__ bool bvh_hit(const GpuScene &scene, const GpuRay &ray, float t_min, float t_max, GpuHitRecord &rec) {
  int stack[64]; // hardcoded should be fine for most cases
  int top = 0;
  stack[top++] = scene.bvh_root;

  bool hit_anything = false;
  float closest = t_max;

  while (top > 0) {
    int idx = stack[--top];
    if (idx < 0 || idx >= scene.num_bvh_nodes) continue;

    const GpuBVHNode &node = scene.bvh_nodes[idx];
    if (!aabb_hit(node, ray, t_min, closest)) continue;

    if (node.prim_count > 0) {
      for (int i = node.prim_start; i < node.prim_start + node.prim_count; ++i) {
        GpuHitRecord tmp{};
        bool hit = false;

        if (scene.primitives[i].type == GpuGeomType::SPHERE)
          hit = sphere_hit(scene.primitives[i].sphere, ray, t_min, closest, tmp);
        else
          hit = triangle_hit(scene.primitives[i].triangle, ray, t_min, closest, tmp);

        if (hit) {
          hit_anything = true;
          closest = tmp.t;
          rec = tmp;
          rec.mat_id = scene.primitives[i].mat_id;
        }
      }
    } else {
      if (node.right >= 0) stack[top++] = node.right;
      if (node.left >= 0) stack[top++] = node.left;
    }
  }
  return hit_anything;
}

__device__ bool mat_scatter(const GpuMaterial &mat, const GpuRay &ray_in, const GpuHitRecord &rec, GpuVec3 &attenuation, GpuRay &scattered, curandState *rng) {
  if (mat.type == GpuMatType::LAMBERTIAN) {
    // build ONB around normal
    GpuVec3 w = rec.normal;
    GpuVec3 a = (fabsf(w.x) > 0.9f) ? GpuVec3{0,1,0} : GpuVec3{1,0,0};
    GpuVec3 v = w.cross(a).normalize();
    GpuVec3 u = w.cross(v);
    // cosine weighted direction
    GpuVec3 d = rand_cosine_direction(rng);
    GpuVec3 dir = u*d.x + v*d.y + w*d.z;
    scattered   = {rec.point + rec.normal * 1e-3f, dir};
    attenuation = mat.albedo;
    return true;
  } 
  
  else if (mat.type == GpuMatType::METAL) {
    GpuVec3 unit = ray_in.direction.normalize();
    float   d    = unit.dot(rec.normal);
    GpuVec3 refl = unit - rec.normal * (2.0f * d);
    GpuVec3 fuzz = rand_in_unit_sphere(rng) * mat.fuzz;
    scattered    = {rec.point + rec.normal * 1e-4f, refl + fuzz};
    attenuation  = mat.albedo;
    return scattered.direction.dot(rec.normal) > 0;
  } 
  
  else if (mat.type == GpuMatType::DIELECTRIC) {
    attenuation  = {1,1,1};
    float ratio  = rec.front_face ? (1.0f/mat.ir) : mat.ir;
    GpuVec3 unit = ray_in.direction.normalize();
    float cos_t  = fminf(-unit.dot(rec.normal), 1.0f);
    float sin_t  = sqrtf(1.0f - cos_t*cos_t);
    bool cannot  = ratio * sin_t > 1.0f;
    // Schlick approximation
    float r0 = (1-ratio)/(1+ratio); r0 = r0*r0;
    float refl = r0 + (1-r0)*powf(1-cos_t, 5);
    GpuVec3 dir;

    if (cannot || refl > rand_f(rng)) dir = unit - rec.normal * (2.0f * unit.dot(rec.normal));
    else {
    GpuVec3 perp = (unit + rec.normal * cos_t) * ratio;
    GpuVec3 para = rec.normal * (-sqrtf(fabsf(1.0f - perp.dot(perp))));
    dir = perp + para;
    }

    scattered = {rec.point, dir};
    return true;

  } else if (mat.type == GpuMatType::DIFFUSE_LIGHT) {
    return false;
  }
  return false;
}

// MIS helpers
// scattering pdf for lambertian
__device__ float scattering_pdf(const GpuMaterial &mat, const GpuHitRecord &rec, const GpuRay &scattered) {
  if (mat.type == GpuMatType::LAMBERTIAN) {
    float cos_t = rec.normal.dot(scattered.direction.normalize());
    return cos_t < 0 ? 0.0f : cos_t/3.14159265f;
  }
  return 0.0f;
}

// power heuristic
__device__ float power_heuristic(float pdf_a, float pdf_b) {
  float a2 = pdf_a * pdf_a;
  float b2 = pdf_b * pdf_b;
  return (a2 + b2) < 1e-10f ? 0.0f : a2 / (a2 + b2);
}

// direct light NEE + MIS
__device__ GpuVec3 direct_light(const GpuScene &scene, const GpuHitRecord &rec, const GpuRay &ray_in, curandState *rng) {
  GpuVec3 result(0,0,0);
  const GpuMaterial &mat = scene.materials[rec.mat_id];

  for (int i = 0; i < scene.num_lights; ++i) {
    const GpuLight &light = scene.lights[i];

    GpuVec3 to_light_dir;
    GpuVec3 emission;
    float   dist;
    float   light_pdf;

    if (light.type == GpuLightType::DIRECTIONAL) {
      to_light_dir = light.direction * -1.0f;
      dist = 1e6f;
      emission = light.color * light.intensity;
      light_pdf = 1.0f;
    } 
    else if (light.type == GpuLightType::SPHERE_AREA) {
      // sample random point on sphere surface
      GpuVec3 rand_dir = rand_in_unit_sphere(rng).normalize();
      GpuVec3 pt = light.position + rand_dir * light.radius;
      GpuVec3 diff = pt - rec.point;
      dist = diff.norm();
      to_light_dir = diff * (1.0f / dist);
      float cos_l = fabsf(rand_dir.dot(GpuVec3{-to_light_dir.x, -to_light_dir.y, -to_light_dir.z}));
      float area = 4.0f * 3.14159265f * light.radius * light.radius;
      light_pdf = (cos_l < 1e-6f) ? 0.0f : (dist*dist) / (cos_l * area);
      emission  = light.color * light.intensity;
    } 
    else continue;

    if (light_pdf <= 0.0f) continue;

    float cos_theta = rec.normal.dot(to_light_dir);
    if (cos_theta <= 0.0f) continue;

    // shadow ray
    GpuRay shadow{rec.point + rec.normal * 1e-3f, to_light_dir};
    GpuHitRecord shadow_rec{};
    if (bvh_hit(scene, shadow, 0.001f, dist - 0.01f, shadow_rec)) {
      if (!scene.materials[shadow_rec.mat_id].is_emissive) continue;
    }

    GpuVec3 attenuation = mat.albedo;

    // MIS weight
    GpuRay to_light_ray{rec.point, to_light_dir};
    float bsdf_pdf  = scattering_pdf(mat, rec, to_light_ray);
    float mis = (bsdf_pdf > 0) ? power_heuristic(light_pdf, bsdf_pdf) : 1.0f;
    result = result + emission * attenuation * (cos_theta * mis / light_pdf);
  }
  return result;
}

// for emmisve hits, total light calculation
__device__ float total_light_pdf(const GpuScene &scene, const GpuVec3 &from, const GpuVec3 &dir) {
  float total = 0.0f;
  for (int i = 0; i < scene.num_lights; ++i) {
    const GpuLight &light = scene.lights[i];
    if (light.type == GpuLightType::SPHERE_AREA) {
      GpuVec3 oc = from - light.position;
      float   a  = dir.dot(dir);
      float   b  = oc.dot(dir);
      float   c  = oc.dot(oc) - light.radius * light.radius;
      float disc = b*b - a*c;
      if (disc < 0) continue;
      float val = 1.0f - light.radius*light.radius / oc.dot(oc);
      if (val <= 0) continue;
      float cos_max    = sqrtf(val);
      float solid_angle = 2.0f * 3.14159265f * (1.0f - cos_max);
      total += (solid_angle > 0) ? 1.0f / solid_angle : 0.0f;
    }
    // directional lights are delta — pdf=0 for random directions
  }
  return total;
}

// ray color function - not recursive to prevent gpu stack overflow
__device__ GpuVec3 ray_color(GpuRay ray, const GpuScene &scene, int max_depth, curandState *rng) {
  GpuVec3 throughput{1,1,1};
  GpuVec3 result{0,0,0};
  float   prev_bsdf_pdf = -1.0f; // -1 = camera ray / specular

  for (int depth = 0; depth < max_depth; ++depth) {
    GpuHitRecord rec{};
    if (!bvh_hit(scene, ray, 0.001f, 1e30f, rec)) {
      // missed — sky gradient
      GpuVec3 unit = ray.direction.normalize();
      float t = 0.5f * (unit.y + 1.0f);
      GpuVec3 sky = GpuVec3{1,1,1} * (1-t) + GpuVec3{0.5f, 0.7f, 1.0f} * t;
      float scale = (prev_bsdf_pdf < 0) ? 1.0f : 0.3f;
      result = result + throughput * sky * scale;
      break;
    }

    const GpuMaterial &mat = scene.materials[rec.mat_id];

    // emissive hit — MIS weighted
    if (mat.is_emissive) {
      if (prev_bsdf_pdf < 0) result = result + throughput * mat.emit_color;
      else {
        // diffuse bounce — weight against NEE
        float lp  = total_light_pdf(scene, ray.origin, ray.direction);
        float mis = (lp > 0) ? power_heuristic(prev_bsdf_pdf, lp) : 1.0f;
        result = result + throughput * mat.emit_color * mis;
      }
      break;
    }

    // NEE direct lighting at every diffuse bounce
    if (!mat.is_transmissive) result = result + throughput * direct_light(scene, rec, ray, rng);
    
    // scatter for next bounce
    GpuVec3 attenuation;
    GpuRay  scattered;
    if (!mat_scatter(mat, ray, rec, attenuation, scattered, rng)) break;

    float bsdf_pdf = scattering_pdf(mat, rec, scattered);
    if (bsdf_pdf > 0) {
      float cos_theta = fmaxf(0.0f, rec.normal.dot(scattered.direction.normalize()));
      throughput = throughput * attenuation * (cos_theta / bsdf_pdf);
    } else throughput = throughput * attenuation; // specular
    prev_bsdf_pdf = (bsdf_pdf > 0) ? bsdf_pdf : -1.0f;
    ray = scattered;

    // Russian roulette after depth 3 — unbiased early termination
    if (depth > 3) {
      float p = fmaxf(throughput.x, fmaxf(throughput.y, throughput.z));
      if (rand_f(rng) > p) break;
      throughput = throughput * (1.0f / p);
    }
  }
  return result;
}

// Camera ray
__device__ GpuRay get_camera_ray(const GpuCamera &cam, float s, float t, curandState *rng) {
    GpuVec3 dir = cam.lower_left_corner + cam.horizontal * s + cam.vertical   * t - cam.origin;
    return {cam.origin, dir};
}

// Main renderer kernel
__global__ void render_kernel(float *fb, int width, int height, int samples, int max_depth, GpuCamera camera, GpuScene scene) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= width ||y >= height) return;

  int pixel = y * width + x;

  // unique rng state per pixel
  curandState rng;
  curand_init((unsigned long long) pixel + 1, 0, 0, &rng);

  // Welford online mean and variance algorithm for adaptive sampling
  GpuVec3 mean(0,0,0);
  GpuVec3 M2(0,0,0); // sum deviation^2
  int n = 0;

  const int MIN_SAMPLES = 16;
  const int CHECK_EVERY = 4;
  const float THRESHOLD = 0.01f;

  for (int s = 0; s < samples; ++s) {
    float u = (x + curand_uniform(&rng)) / (float)(width - 1);
    float v = (y + curand_uniform(&rng)) / (float)(height - 1);
    GpuRay ray = get_camera_ray(camera, u, v, &rng);
    GpuVec3 color = ray_color(ray, scene, max_depth, &rng);

    // welford algorithm
    n++;
    GpuVec3 delta = color - mean;
    mean = mean + delta * (1.0f / n);
    GpuVec3 delta2 = color - mean;
    M2 = M2 + GpuVec3(delta.x*delta2.x, delta.y*delta2.y, delta.z*delta2.z);

    if (n >= MIN_SAMPLES && (n % CHECK_EVERY == 0)) {
      GpuVec3 variance = M2 * (1.0f / n);
      if (variance.x < THRESHOLD && variance.y < THRESHOLD && variance.z < THRESHOLD) break;
    }
  }
  
  // gamma correction
  fb[pixel*3+0] = fmaxf(0.0f, mean.x);
  fb[pixel*3+1] = fmaxf(0.0f, mean.y);
  fb[pixel*3+2] = fmaxf(0.0f, mean.z);
}


// main host entry point called in main_cuda.cu
void cuda_render(const CudaRenderParams &params, const GpuScene &host_scene, const std::vector<GpuLight> &host_lights, std::vector<float> &out_fb) {
  int W = params.width;
  int H = params.height;
  out_fb.resize(W*H*3);

  // upload the scene to gpu
  GpuScene d_scene = host_scene;
  CUDA_CHECK(cudaMalloc(&d_scene.primitives, host_scene.num_primitives * sizeof(GpuPrimitive)));
  CUDA_CHECK(cudaMemcpy(d_scene.primitives, host_scene.primitives, host_scene.num_primitives*sizeof(GpuPrimitive), cudaMemcpyHostToDevice));

  CUDA_CHECK(cudaMalloc(&d_scene.bvh_nodes, host_scene.num_bvh_nodes * sizeof(GpuBVHNode)));
  CUDA_CHECK(cudaMemcpy(d_scene.bvh_nodes, host_scene.bvh_nodes, host_scene.num_bvh_nodes * sizeof(GpuBVHNode), cudaMemcpyHostToDevice));

  CUDA_CHECK(cudaMalloc(&d_scene.materials, host_scene.num_materials * sizeof(GpuMaterial)));
  CUDA_CHECK(cudaMemcpy(d_scene.materials, host_scene.materials, host_scene.num_materials * sizeof(GpuMaterial), cudaMemcpyHostToDevice));
  
  if (!host_lights.empty()) {
    CUDA_CHECK(cudaMalloc(&d_scene.lights, host_lights.size() * sizeof(GpuLight)));
    CUDA_CHECK(cudaMemcpy(d_scene.lights, host_lights.data(), host_lights.size() * sizeof(GpuLight), cudaMemcpyHostToDevice));
  }
  d_scene.num_lights = (int)host_lights.size();

  // output framebuffer GPU
  float *d_fb;
  CUDA_CHECK(cudaMalloc(&d_fb, W * H * 3 * sizeof(float)));

  // launch
  dim3 block(16, 16);
  dim3 grid((W + 15) / 16, (H + 15) / 16);
  
  render_kernel<<<grid, block>>>( d_fb, W, H, params.samples_per_pixel, params.max_depth, params.camera, d_scene);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());

  // download to host
  CUDA_CHECK(cudaMemcpy(out_fb.data(), d_fb, W * H * 3 * sizeof(float), cudaMemcpyDeviceToHost));

  // cleanup all memory
  cudaFree(d_fb);
  cudaFree(d_scene.primitives);
  cudaFree(d_scene.materials);
  cudaFree(d_scene.bvh_nodes);
  cudaFree(d_scene.lights);
}