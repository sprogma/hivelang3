#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "runtime_api.h"
#include "remote.h"
#include "runtime.h"

#include "providers.h"
#include "gpu_subsystem.h"

#include "x64/x64.h"
#include "gpu/gpu.h"


struct cast_info
{
    int64_t object;
    int64_t to;
    int64_t from;
    int64_t known_size;
    int64_t known_param;
    int64_t known_type;
    int64_t new_id;
};

struct running_worker
{
    int64_t state;
    void *returnAddress;
    void *rbpValue;
    struct cast_info ci;
};

struct waiting_pages
{
    struct cast_info ci;
};

struct waiting_type
{
    struct cast_info ci;
};

struct wait_query_info
{
    struct cast_info ci;
    
    int64_t offset;
    int64_t size;
    BYTE id[BROADCAST_ID_LENGTH];
    int64_t repeat_timeout;
};

struct wait_query_info_answer
{
    struct cast_info ci;
    void *data;
};


int64_t anyCastStates(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue);


//@regQuery WK_STATE_GET_OBJECT_SIZE castOnQueryObject
//@regQuery WK_STATE_GET_OBJECT_DATA castOnQueryObject
int64_t castOnQueryObject(struct waiting_worker *w, int64_t object, int64_t offset, int64_t size, void *data, int64_t *rdiValue)
{
    struct wait_query_info *info = w->state_data;
    if (info->ci.object == object && myAbs(info->size) == size && info->offset == offset)
    {
        switch (w->state)
        {       
        case WK_STATE_GET_OBJECT_SIZE:
            w->state = WK_STATE_GET_OBJECT_SIZE_RESULT; break;
        case WK_STATE_GET_OBJECT_DATA:
            w->state = WK_STATE_GET_OBJECT_DATA_RESULT; break;
        } 
        
        struct wait_query_info_answer *info = myMalloc(sizeof(*info));
        *info = (struct wait_query_info_answer){
            .ci = ((struct wait_query_info *)w->state_data)->ci,
            .data = data,
        };
        myFree(w->state_data);
        w->state_data = info;
        return anyCastStates(w, GetTicks(), rdiValue);
    }
    return 0;
}

//@reg WK_STATE_GET_OBJECT_SIZE anyCastStates
//@reg WK_STATE_GET_OBJECT_SIZE_RESULT anyCastStates
//@reg WK_STATE_GET_OBJECT_DATA anyCastStates
//@reg WK_STATE_GET_OBJECT_DATA_RESULT anyCastStates
//@reg WK_STATE_CAST_WAIT_PAGES anyCastStates
int64_t anyCastStates(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    (void)ticks;
    struct running_worker *running = NULL;
    int64_t obj;
    int64_t to;
    int64_t from;
    int64_t known_size;
    int64_t known_param;
    int64_t known_type;
    int64_t new_id;
    #define loadFrom(s) { \
        obj = (s)->ci.object; \
        to = (s)->ci.to; from = (s)->ci.from; \
        known_size = (s)->ci.known_size; \
        known_param = (s)->ci.known_param; \
        known_type = (s)->ci.known_type; \
        new_id = (s)->ci.new_id; \
    }
    #define EXTRA_INIT
    #define pauseIfNeeded(s, T, ...) { \
        T *info = myMalloc(sizeof(*info)); \
        *info = (T){.ci = { \
            .object=obj, \
            .to=to, \
            .from=from, \
            .known_size=known_size, \
            .known_param=known_param, \
            .known_type=known_type, \
            .new_id=new_id} __VA_OPT__(,) __VA_ARGS__}; \
        EXTRA_INIT \
        if (w == NULL) { \
            universalPauseWorker(running->returnAddress, running->rbpValue, s, info); \
            struct thread_data* lc_data = TlsGetValue(dwTlsIndex); \
            longjmpUN(&lc_data->ShedulerBuffer, 1); \
        } else { \
            myFree(w->state_data); \
            w->state_data = info; \
            w->state = s; \
        } \
    }
    
    switch (w->state)
    {
    
    case -1:
        running = (struct running_worker *)w;
        w = NULL;
        loadFrom(running);

        if (to == PROVIDER_X64)
        {
            // on CPU we need to get unique ID
            if (0) {
        case WK_STATE_CAST_WAIT_PAGES:
                loadFrom((struct waiting_pages *)w->state_data);
            }
            
            int64_t new_id;
            if (!GetNewObjectId(&new_id))
            {
                pauseIfNeeded(WK_STATE_CAST_WAIT_PAGES, struct waiting_pages)
                return 0;
            }

            log("Get object id = %lld\n", new_id);
        }

        // if object is array - get it size
        if (known_size == 0)
        {
            if (known_type != OBJECT_ARRAY)
            {
                print("Error: no known size for not array object");
                *rdiValue = 0;
                return 1;
            }
            if (from == PROVIDER_GPU)
            {
                struct gpu_object *gpu_obj = (void *)obj;
                known_size = gpu_obj->size;
                known_param = gpu_obj->size / gpu_obj->length;
            }
            else
            {
                if (0) {
            case WK_STATE_GET_OBJECT_SIZE:
                    loadFrom((struct wait_query_info *)w->state_data);
                    // continue quering
                    struct wait_query_info *info = w->state_data;
                    struct object *lc_obj = (void *)GetHashtable(&local_objects, (BYTE *)&info->ci.object, 8, 0);
                    if (lc_obj == 0)
                    {
                        if (ticks > info->repeat_timeout)
                        {                   
                            RequestObjectGet(info->ci.object, info->offset, myAbs(info->size));
                            info->repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT);
                        }
                        return 0;
                    }
                    if (x64QueryLocalObject(&known_size, lc_obj, -16, 6, NULL) == 0)
                    {
                        return 0;
                    }
                    log("got local size answer\n");
                    // now, we know size
                } else {
                    log("Querying object size of %lld\n", obj);
                    // query initialization
                    BYTE *lc_obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&obj, 8, 0);
                    if (lc_obj == NULL)
                    {
                        RequestObjectGet(obj, -16, 6);
                        
                        /* shedule query */
                        #undef EXTRA_INIT
                        #define EXTRA_INIT BCryptGenRandom(NULL, info->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
                        pauseIfNeeded(WK_STATE_GET_OBJECT_SIZE, struct wait_query_info, 
                            .offset = -16,
                            .size = 6,
                            .repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT)
                        )
                        #undef EXTRA_INIT
                        #define EXTRA_INIT
                        return 0;
                    }
                    if (x64QueryLocalObject(&known_size, lc_obj, -8, 6, NULL) == 0)
                    {
                        return 0;
                    }
                    log("got local size answer\n");
                    int64_t x = 0;
                    memcpy(&x, lc_obj - 8, 6);
                    // now, we know size
                }

                if (0) {
            case WK_STATE_GET_OBJECT_SIZE_RESULT:
                    // got result
                    loadFrom((struct wait_query_info_answer *)w->state_data);
                    memcpy(&known_size, ((struct wait_query_info_answer *)w->state_data)->data, 6);
                    log("got remote size answer\n");
                }
            }
        }
        
        log("Know size=%lld [param=%lld]\n", known_size, known_param);

        // now, create new object [and fill it]
        if (to == PROVIDER_X64)
        {
            x64NewObjectUsingPage(known_type, known_size, known_param, &new_id);
            BYTE *lc_obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&new_id, 8, 0);
            if (lc_obj == NULL)
            {
                print("Error: local object was removed too fast :(\n");
                return 1;
            }
            struct gpu_object *go = (void *)obj;
            // download GPU's data into buffer
            int err = clEnqueueReadBuffer(
                SL_queues[SL_main_platform][0],
                go->mem,
                CL_TRUE,
                0,
                known_size,
                lc_obj,
                0, NULL, NULL
            );
            if (err)
            {
                print("Error happen while clEnqueueReadBuffer [%lld]\n", err);
            }
            log("Successifully converted gpu->cpu\n");
        }
        else if (to == PROVIDER_GPU)
        {   
            void *host_data;
            
            // query object to get data
            if (0) {
            case WK_STATE_GET_OBJECT_DATA:
                loadFrom((struct wait_query_info *)w->state_data);
                
                // continue quering
                struct wait_query_info *info = w->state_data;
                struct object *lc_obj = (void *)GetHashtable(&local_objects, (BYTE *)&info->ci.object, 8, 0);
                if (lc_obj == 0)
                {
                    if (ticks > info->repeat_timeout)
                    {                   
                        RequestObjectGet(info->ci.object, info->offset, myAbs(info->size));
                        info->repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT);
                    }
                    return 0;
                }

                if (((BYTE *)lc_obj)[-1] == OBJECT_PROMISE)
                {
                    struct object_promise *p = (struct object_promise *)(lc_obj - DATA_OFFSET(*p));
                    if (!p->ready)
                    {
                        return 0;
                    }
                }
                
                host_data = lc_obj;
            } else {
                // query initialization
                BYTE *lc_obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&obj, 8, 0);
                if (lc_obj == NULL)
                {
                    RequestObjectGet(obj, 0, known_size);
                    
                    /* shedule query */
                    #undef EXTRA_INIT
                    #define EXTRA_INIT BCryptGenRandom(NULL, info->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
                    pauseIfNeeded(WK_STATE_GET_OBJECT_DATA, struct wait_query_info, 
                        .offset = 0,
                        .size = known_size,
                        .repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT)
                    )
                    #undef EXTRA_INIT
                    #define EXTRA_INIT
                    return 0;
                }

                if (((BYTE *)lc_obj)[-1] == OBJECT_PROMISE)
                {
                    struct object_promise *p = (struct object_promise *)(lc_obj - DATA_OFFSET(*p));
                    if (!p->ready)
                    {
                        return 0;
                    }
                }
                
                host_data = lc_obj;
            }

            if (0) {
            case WK_STATE_GET_OBJECT_DATA_RESULT:
                // got result
                loadFrom((struct wait_query_info_answer *)w->state_data);
                host_data = ((struct wait_query_info_answer *)w->state_data)->data;
            }
            
            // create new GPU object
            gpuNewObjectUsingPage(known_type, known_size, known_param, &new_id);
            struct gpu_object *go = (void *)new_id;
            
            // upload data into GPU's buffer
            int err = clEnqueueWriteBuffer(
                SL_queues[SL_main_platform][0], 
                go->mem, 
                CL_TRUE,
                0,
                known_size,
                host_data,
                0, NULL, NULL
            );
            if (err)
            {
                print("Error happen while clEnqueueWriteBuffer [%lld]\n", err);
            }
            log("Successifully converted cpu->gpu\n");
        }
        
        // cast applied
        *rdiValue = new_id;
        print("Return: %lld\n", new_id);
        if (w)
        {
            myFree(w->state_data);
        }
        return 1;
    }
    
    __builtin_unreachable();
}



__attribute__((sysv_abi))
int64_t anyCastProvider(void *obj, int64_t to, int64_t from, int64_t object_size, void *returnAddress, void *rbpValue)
{
    print("convert %p [types %lld -> %lld]\n", obj, from, to);
    if (((from & 0xFF) == PROVIDER_X64 && (to & 0xFF) == PROVIDER_GPU) || 
        ((from & 0xFF) == PROVIDER_GPU && (to & 0xFF) == PROVIDER_X64))
    {
        int64_t new_id;
        struct running_worker wkinfo = {
            .state = -1,
            .returnAddress = returnAddress,
            .rbpValue = rbpValue,
            .ci = {
                .object = (int64_t)obj,
                .to = to & 0xFF,
                .from = from & 0xFF,
                .known_size = from>>8 == OBJECT_ARRAY?0:object_size,
                .known_param = object_size,
                .known_type = from >> 8,
            }
        };
        anyCastStates((struct waiting_worker *)&wkinfo, GetTicks(), &new_id);
        return new_id;
    }
    print("ERROR: not supported conversion between providers %lld to %lld\n", from, to);
    return 0;
}
