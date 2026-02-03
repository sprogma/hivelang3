#ifndef GPU_SUB_H
#define GPU_SUB_H


#define CL_TARGET_OPENCL_VERSION 300

#include "CL/opencl.h"
#include "inttypes.h"


#define MAX_DIMENSIONS 4

struct gpu_kernel_shape_t
{
    uint32_t local_size_used;
    uint32_t dim;
    size_t global_offset[MAX_DIMENSIONS];
    size_t global_size[MAX_DIMENSIONS];
    size_t local_size[MAX_DIMENSIONS];
};


int init_gpu_subsystem();
int gpuFinishQueue(int platform_id, int device_id);
int gpuFinishQueuesOnPlatform(int platform_id);
int gpuRun(int platform_id, int comand_queue_id, cl_kernel kernel, struct gpu_kernel_shape_t config_shape);
cl_mem gpuAlloc(size_t size, cl_mem_flags flags, int *ret_err);
cl_kernel gpuBuildFromText(int platform_id, int device_id, const char *kernel_function_name, const char *source_code, size_t source_length, int *ret_err);

#endif
