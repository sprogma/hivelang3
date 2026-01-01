#include "inttypes.h"
#include "stdio.h"


void *objects[1000];


// void QueryObject(void *dest, int64_t object, int64_t offset, int64_t index)
// {
//     printf("CALL QUERY OBJECT WITH %lld args: ", count);
//     for (int64_t i = 0; i < count; ++i)
//     {
//         printf(" %lld", args[i]);
//     }
//     printf("\n");
//     return 0;
// }
// 
// 
// int64_t PushObject(int64_t count, int64_t *args)
// {
//     printf("CALL PUSH OBJECT WITH %lld args: ", count);
//     for (int64_t i = 0; i < count; ++i)
//     {
//         printf(" %lld", args[i]);
//     }
//     printf("\n");
//     return 0;
// }
// 
// 
// int64_t NewObject(int64_t count, int64_t *args)
// {
//     printf("CALL NEW OBJECT WITH %lld args: ", count);
//     for (int64_t i = 0; i < count; ++i)
//     {
//         printf(" %lld", args[i]);
//     }
//     printf("\n");
//     return 0;
// }
// 
// 
// void CallObject(int64_t worker, int64_t *args)
// {
//     printf("CALL OF WORKER %lld WITH args.\n", worker);
// }

enum relocation_type
{
    DYNAMIC_SYMBOL,
    QUERY_OBJECT,
    PUSH_OBJECT,
    NEW_OBJECT,
    CALL_OBJECT,
};

#define RELOCATION_32BIT        0x0001
#define RELOCATION_64BIT        0x0002
#define RELOCATION_RELATIVE     0x0010
#define RELOCATION_NOT_RELATIVE 0x0020

struct relocation
{
    enum relocation_type type;
    int64_t flags;
    union
    {
        struct {
            const char *library;
            const char *name;
        } dll;
    };
};
 

struct worker
{
    struct relocation *relocations;
    int64_t relocations_len;
    void *code;
};


void LoadWorker(void *code)
{
}

