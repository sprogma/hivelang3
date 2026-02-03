#include "gpu_subsystem.h"
#include "runtime_lib.h"

#define MAX_SL_PLATFORMS 16
#define MAX_SL_DEVICES 8
#define MAX_SL_QUEUES 8

cl_platform_id   SL_platforms[MAX_SL_PLATFORMS];
int              SL_platforms_len;
cl_device_id     SL_devices[MAX_SL_PLATFORMS][MAX_SL_DEVICES];
int              SL_devices_len[MAX_SL_PLATFORMS];
cl_context       SL_context;
cl_command_queue SL_queues[MAX_SL_PLATFORMS][MAX_SL_QUEUES];
int              SL_queues_len[MAX_SL_PLATFORMS];

int SL_best_platform = 0;
int SL_best_platform_value = 0;


int init_gpu_subsystem()
{
    // TODO: make config
    cl_device_type device_flags = CL_DEVICE_TYPE_ALL;
    
    int err;
    unsigned int read_count;

    // initialize cl (int * == unsigned int *, if number is little.)
    err = clGetPlatformIDs(MAX_SL_PLATFORMS, SL_platforms, (unsigned int *)&SL_platforms_len);
    if (err) { return err; }
    if (SL_platforms_len > MAX_SL_PLATFORMS)
    {
        SL_platforms_len = MAX_SL_PLATFORMS;
    }
    print("Got at least %lld platforms\n", (int64_t)SL_platforms_len);
    // print them
    for (int i = 0; i < SL_platforms_len; ++i)
    {
        size_t allocated = 100;
        size_t info_len = 0;
        char *info = myMalloc(100), *tmp = NULL;
        print("Platform %lld:\n", (int64_t)i);
        #define PRINT(type, ...) \
        clGetPlatformInfo(SL_platforms[i], type, 0, NULL, &info_len); /* get size */ \
        if (allocated < info_len) \
        { \
            allocated = info_len; \
            tmp = info; info = myRealloc(info, info_len); \
            if (!info) { myFree(tmp); return 57; }\
        } \
        clGetPlatformInfo(SL_platforms[i], type, info_len, info, NULL); /* get value */ \
        print(__VA_ARGS__, info)
        #define PRINV(type, typename, ...) \
        clGetPlatformInfo(SL_platforms[i], type, 0, NULL, &info_len); /* get size */ \
        if (allocated < info_len) \
        { \
            allocated = info_len; \
            tmp = info; info = myRealloc(info, info_len); \
            if (!info) { myFree(tmp); return 57; }\
        } \
        clGetPlatformInfo(SL_platforms[i], type, info_len, info, NULL); /* get value */ \
        print(__VA_ARGS__, *(typename *)info)
        PRINT(CL_PLATFORM_NAME,                                 "\t%lld.Name             : %s\n", (int64_t)i);
        PRINT(CL_PLATFORM_VENDOR,                               "\t%lld.Vendor           : %s\n", (int64_t)i);
        PRINT(CL_PLATFORM_VERSION,                              "\t%lld.Version          : %s\n", (int64_t)i);
        PRINT(CL_PLATFORM_PROFILE,                              "\t%lld.Profile          : %s\n", (int64_t)i);
        PRINT(CL_PLATFORM_EXTENSIONS,                           "\t%lld.Extensions       : %s\n", (int64_t)i);
        PRINV(CL_PLATFORM_HOST_TIMER_RESOLUTION, unsigned long, "\t%lld.Timer Resolution : %lld\n", (int64_t)i);
        #undef PRINT
        #undef PRINV
        myFree(info);
        print("\n");

        int value = 0;
        value += 0;
        if (value > SL_best_platform_value)
        {
            SL_best_platform = i;
            SL_best_platform_value = value;
        }
    }

    print("\n");

    // load all devices
    int count = 0;
    for (int i = 0; i < SL_platforms_len; ++i)
    {
        err = clGetDeviceIDs(SL_platforms[i],
                       device_flags,
                       MAX_SL_DEVICES,
                       SL_devices[i],
                       &read_count);
        if (err) { return err; }
        if (read_count > MAX_SL_DEVICES)
        {
            read_count = MAX_SL_DEVICES;
        }
        print("platform %lld: Got at least %lld devices\n", (int64_t)i, (int64_t)read_count);
        SL_devices_len[i] = read_count;
        // print them
        for (int dev = 0; dev < SL_devices_len[i]; ++dev)
        {
            size_t allocated = 100;
            size_t info_len = 0;
            unsigned dims = -1;
            char *info = myMalloc(100), *tmp = NULL;
            print("Device %lld:\n", (int64_t)i);
            #define PRINT(type, ...) \
            clGetDeviceInfo(SL_devices[i][dev], type, 0, NULL, &info_len); /* get size */ \
            if (allocated < info_len) \
            { \
                allocated = info_len; \
                tmp = info; info = myRealloc(info, info_len); \
                if (!info) { myFree(tmp); return 57; }\
            } \
            clGetDeviceInfo(SL_devices[i][dev], type, info_len, info, NULL); /* get value */ \
            print(__VA_ARGS__, info)
            #define PRINV(type, typename, ...) \
            clGetDeviceInfo(SL_devices[i][dev], type, 0, NULL, &info_len); /* get size */ \
            if (allocated < info_len) \
            { \
                allocated = info_len; \
                tmp = info; info = myRealloc(info, info_len); \
                if (!info) { myFree(tmp); return 57; }\
            } \
            clGetDeviceInfo(SL_devices[i][dev], type, info_len, info, NULL); /* get value */ \
            print(__VA_ARGS__, *(typename *)info)
            #define PRINS(type, typename, ...) \
            clGetDeviceInfo(SL_devices[i][dev], type, 0, NULL, &info_len); /* get size */ \
            if (allocated < info_len) \
            { \
                allocated = info_len; \
                tmp = info; info = myRealloc(info, info_len); \
                if (!info) { myFree(tmp); return 57; }\
            } \
            clGetDeviceInfo(SL_devices[i][dev], type, info_len, info, NULL); /* get value */ \
            print(__VA_ARGS__, *(typename *)info); \
            dims = *(unsigned *)info;
            #define PRINI(type, typename, ...) \
            clGetDeviceInfo(SL_devices[i][dev], type, 0, NULL, &info_len); /* get size */ \
            if (allocated < info_len) \
            { \
                allocated = info_len; \
                tmp = info; info = myRealloc(info, info_len); \
                if (!info) { myFree(tmp); return 57; }\
            } \
            clGetDeviceInfo(SL_devices[i][dev], type, info_len, info, NULL); /* get value */ \
            for (uint32_t d = 0; d < dims; ++d) \
            { \
                print(__VA_ARGS__, d, ((typename *)info)[d]); \
            }
            PRINT(CL_DEVICE_NAME,                                     "\t%lld.%lld.Name                      : %s\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_AVAILABLE, int,                           "\t%lld.%lld.Available                 : %llx\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_TYPE, unsigned,                           "\t%lld.%lld.Type                      : %llx\n", (int64_t)i, (int64_t)dev);
            PRINT(CL_DEVICE_VENDOR,                                   "\t%lld.%lld.Vendor                    : %s\n", (int64_t)i, (int64_t)dev);
            PRINT(CL_DRIVER_VERSION,                                  "\t%lld.%lld.Driver Version            : %s\n", (int64_t)i, (int64_t)dev);
            PRINT(CL_DEVICE_VERSION,                                  "\t%lld.%lld.Device Version            : %s\n", (int64_t)i, (int64_t)dev);
            PRINT(CL_DEVICE_PROFILE,                                  "\t%lld.%lld.Profile                   : %s\n", (int64_t)i, (int64_t)dev);
            PRINT(CL_DEVICE_EXTENSIONS,                               "\t%lld.%lld.Extensions                : %s\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_MAX_COMPUTE_UNITS, unsigned,              "\t%lld.%lld.Max Compute Units         : %llu\n", (int64_t)i, (int64_t)dev);
            PRINS(CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, unsigned,       "\t%lld.%lld.Max Item Dimensions       : %llu\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_MAX_WORK_GROUP_SIZE, size_t,              "\t%lld.%lld.Max Group Size            : %llu\n", (int64_t)i, (int64_t)dev);
            PRINI(CL_DEVICE_MAX_WORK_ITEM_SIZES, size_t,              "\t\t%lld.%lld.Max Item Sizes[%lld]        : %llu\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_MAX_CLOCK_FREQUENCY, unsigned,            "\t%lld.%lld.Max Clock Frequency       : %llu\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_MAX_MEM_ALLOC_SIZE, unsigned long,        "\t%lld.%lld.Max Mem Alloc Size        : %llu\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, unsigned,      "\t%lld.%lld.Global Mem Cacheline Size : %llu\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, unsigned long,     "\t%lld.%lld.Global Mem Cache Size     : %llu\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_GLOBAL_MEM_SIZE, unsigned long,           "\t%lld.%lld.Global Mem Size           : %llu\n", (int64_t)i, (int64_t)dev);
            PRINV(CL_DEVICE_LOCAL_MEM_SIZE, unsigned long,            "\t%lld.%lld.Local Mem Size            : %llu\n", (int64_t)i, (int64_t)dev);
            #undef PRINT
            #undef PRINV
            #undef PRINS
            #undef PRINI
            myFree(info);
            print("\n");
        }
        count += read_count;
    }
    
    print("At end have %lld devices.\n", count);


    int platform_id = SL_best_platform;

    print("Init form platform %lld [auto selected]\n", platform_id);
    
    SL_context = clCreateContext(NULL,
                                 SL_devices_len[platform_id],
                                 SL_devices[platform_id],
                                 NULL, // using no function for errors detection
                                 NULL, // and no it's data
                                 &err);
    if (err) { return err; }
    for (int i = 0; i < SL_devices_len[platform_id]; ++i)
    {
        SL_queues[platform_id][i] = clCreateCommandQueueWithProperties(SL_context,
                                                         SL_devices[platform_id][i],
                                                         NULL,
                                                         &err);
        if (err) { return err; }
    }
    return 0;
}

cl_kernel gpuBuildFromText(int platform_id, int device_id, const char *kernel_function_name, const char *source_code, size_t source_length, int *ret_err)
{
    int err;
    cl_program program = clCreateProgramWithSource(SL_context,
                                                   1,
                                                   &source_code,
                                                   &source_length,
                                                   &err);
    if (err) { if (ret_err) *ret_err = err; return NULL; }
    err = clBuildProgram(program,
                         0,
                         NULL, // SL_devices[platform_id] + device_id compile on all devices
                         // https://registry.khronos.org/OpenSL/specs/3.0-unified/html/OpenCL_API.html#compiler-options
                         "-cl-single-precision-constant -cl-unsafe-math-optimizations",
                         NULL,  // using no function for errors detection
                         NULL); // and no it's data

    if (err)
    {
        print("BuildProgram Error: %lld\n", err);
        size_t len = 0;
        cl_int ret = CL_SUCCESS;
        ret = clGetProgramBuildInfo(program, SL_devices[platform_id][device_id], CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
        if (ret)
        {
            print("BuildInfo Error: %lld\n", ret);
            * ret_err = 1;
            return NULL;
        }
        len += 10;
        char *buffer = myMalloc(len);
        memset(buffer, 0, len);
        ret = clGetProgramBuildInfo(program, SL_devices[platform_id][device_id], CL_PROGRAM_BUILD_LOG, len, buffer, NULL);
        if (ret)
        {
            print("BuildInfo2 Error: %lld\n", ret);
            * ret_err = 1;
            return NULL;
        }

        print("Error Info: code=%lld: info=%s\n", err, buffer);

        myFree(buffer);
        if (ret_err)
        {
            *ret_err = err;
        }
        return NULL;
    }
    cl_kernel kernel = clCreateKernel(program,
                                      kernel_function_name,
                                      &err);
    if (err) { if (ret_err) *ret_err = err; return NULL; }

    if (ret_err) *ret_err = 0;
    return kernel;
}

cl_mem gpuAlloc(size_t size, cl_mem_flags flags, int *ret_err)
{
    return clCreateBuffer(SL_context,
                          flags,
                          size,
                          NULL,
                          ret_err);
}

int gpuRun(int platform_id, int comand_queue_id, cl_kernel kernel, struct gpu_kernel_shape_t config_shape)
{
    return clEnqueueNDRangeKernel(SL_queues[platform_id][comand_queue_id],
                                  kernel,
                                  config_shape.dim,
                                  config_shape.global_offset,
                                  config_shape.global_size,
                                  (config_shape.local_size_used ? config_shape.local_size : NULL),
                                  0,
                                  NULL,
                                  NULL);
}

int gpuFinishQueuesOnPlatform(int platform_id)
{
    int err;
    for (int i = 0; i < SL_queues_len[platform_id]; ++i)
    {
        err = clFinish(SL_queues[platform_id][i]);
        if (err) { return err; }
    }
    return 0;
}

int gpuFinishQueue(int platform_id, int device_id)
{
    return clFinish(SL_queues[platform_id][device_id]);
}

