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


#define MAX_SL_PLATFORMS 16
#define MAX_SL_DEVICES 8
#define MAX_SL_QUEUES 8

extern cl_platform_id   SL_platforms[MAX_SL_PLATFORMS];
extern int              SL_platforms_len;
extern cl_device_id     SL_devices[MAX_SL_PLATFORMS][MAX_SL_DEVICES];
extern int              SL_devices_len[MAX_SL_PLATFORMS];
extern cl_context       SL_context;
extern cl_command_queue SL_queues[MAX_SL_PLATFORMS][MAX_SL_QUEUES];
extern int              SL_queues_len[MAX_SL_PLATFORMS];

extern int              SL_main_platform;

int init_gpu_subsystem();
int gpuFinishQueue(int platform_id, int device_id);
int gpuFinishQueuesOnPlatform(int platform_id);
int gpuRun(int platform_id, int comand_queue_id, cl_kernel kernel, struct gpu_kernel_shape_t config_shape);
cl_mem gpuAlloc(size_t size, cl_mem_flags flags, int *ret_err);
cl_kernel gpuBuildFromText(int platform_id, int device_id, const char *kernel_function_name, const char *source_code, size_t source_length, int *ret_err);

#endif
