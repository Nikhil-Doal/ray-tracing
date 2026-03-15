#pragma once
#include "cuda_scene.cuh"
#include <vector>

struct CudaRenderParams {
  int width; 
  int height;
  int samples_per_pixel;
  int max_depth;
  GpuCamera camera;
};

// main entry point called from main_cuda.cu
// uploads scene to gpu, launches kerner and downloads result
void cuda_render(const CudaRenderParams &params, const GpuScene &host_scene, const std::vector<GpuLight> &host_lights, std::vector<float> &out_framebuffer);
// out_framebuffer flat rbg floats -> size = 3wh
// already gamma corrected -> passes to save_png with samples = 1
