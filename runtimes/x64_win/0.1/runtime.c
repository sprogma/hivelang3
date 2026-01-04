#include "inttypes.h"
#include "inttypes.h"
#include "stdio.h"
#include "windows.h"

extern int fastPushObject(void);
extern int fastQueryObject(void);
extern int fastNewObject(void);

extern void callExample(void *);

#define OBJECT_PIPE    0x01
#define OBJECT_PROMISE 0x02
#define OBJECT_OBJECT  0x03

struct object
{
    int32_t type;
};

struct promise
{
    int32_t type;
    int32_t ready;
    BYTE data[];
};


struct object *object_array[1000] = {};
int64_t object_array_len = 0;


int64_t NewObject(int64_t type, int64_t size)
{
    switch (type)
    {
        case OBJECT_PROMISE:
        {
            printf("Promise for size %lld allocated\n", size);
            struct promise *res = malloc(sizeof(*res) + size);
            res->type = type;
            res->ready = 0;
            object_array[object_array_len++] = (struct object *)res;
            return (int64_t)res;
        }
        default:
            printf("Wrong type in NewObject\n");
    }
    return -1;
}

void PushObject(){}
void QueryObject(){}
void CallObject(){}

// // if dest == NULL - return into rax
// int64_t QueryObject(void *dest, int64_t object, int64_t offset, int64_t size)
// {
//     printf("CALL QUERY OBJECT WITH %p <- %lld[%lld:%lld]\n", dest, object, offset, size);
//     return 0;
// }
// 
// // if size < 0 -> src is constant value [int64_t]
// void PushObject(int64_t object, int64_t offset, int64_t size, void *src)
// {
//     printf("CALL PUSH OBJECT WITH %lld[%lld:%lld] <- %p\n", object, offset, size, src);
// }
// 
// // size is size of element in pipe
// #define NEW_OBJECT_PIPE    0x01
// 
// // size is size of element in promise
// #define NEW_OBJECT_PROMISE 0x02
// 
// // size is total size of object
// #define NEW_OBJECT_OBJECT  0x03
// 
// // return new object [id] in rax
// int64_t NewObject(int64_t type, int64_t size)
// {
//     printf("CALL NEW OBJECT WITH rax <- new %lld with param %lld\n", type, size);
//     return 0;
// }
// 
// void CallObject(int64_t worker)
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


typedef uint8_t BYTE;


// executable structure 0.1:
/*

    prefix
        "HIVE" ?

    version
        i64 // = main_version * 1000 + low_version
    
    header:
        i64 address of code

        array of external symbols: [all space up to start of code]
            i8 symbol_type
            ... [[data]]
    code:
        raw bytes

*/
void *LoadWorker(BYTE *file, int64_t fileLength, int64_t *res_len)
{
    /* read prefix */
    if (file[0] != 'H' || file[1] != 'I' || file[2] != 'V' || file[3] != 'E')
    {
        printf("Error: this isn't hive executable\n");
        return NULL;
    }
    uint64_t version = *(uint64_t *)&file[4];
    if (version / 1000 != 0)
    {
        printf("Error: this is executable of not 0 version [%llu]\n", version / 1000);
        return NULL;
    }
    uint64_t codePosition = *(uint64_t *)&file[12];
    /* read code */
    void *mem = VirtualAlloc(NULL, fileLength - codePosition, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (mem == NULL)
    {
        printf("Error: winapi error %ld\n", GetLastError());
        return NULL;
    }
    memcpy(mem, file + codePosition, fileLength - codePosition);
    if (res_len) { *res_len = fileLength - codePosition; }
    /* read header */
    BYTE *pos = file + 20;
    while (pos < codePosition + file)
    {
        BYTE type = *pos++;
        switch (type)
        {
            case 0: // PushObject
            case 1: // QueryObject
            case 2: // NewObject
            {
                // read positions and replace calls
                int64_t count = *(int64_t *)pos;
                pos += 8;
                for (int64_t i = 0; i < count; ++i)
                {
                    printf("set to %lld one of %p %p %p\n", *(int64_t *)pos, fastNewObject, fastQueryObject, fastPushObject);
                    uint64_t *callPosition = (uint64_t *)(mem + *(int64_t *)pos);
                    pos += 8;
                    switch (type)
                    {
                        case 0: *callPosition = (uint64_t)&fastPushObject; break;
                        case 1: *callPosition = (uint64_t)&fastQueryObject; break;
                        case 2: *callPosition = (uint64_t)&fastNewObject; break;
                    }
                }
                break;
            }
            case 3: // CallObject
                printf("Error: CallObject is unsupported in version 0.1\n");
                break;
            case 4: // DLL call
                /*   
                    i64 name_len
                    byte[] name
                    i64 positions length
                    i64[] positions
                */
                printf("Error: DLL Calls are unsupported in version 0.1\n");
                break;
            default:
                printf("Error: unknown header type %u\n", type);
                break;
        }
    }
    return mem;
}
 

struct worker
{
    struct relocation *relocations;
    int64_t relocations_len;
    void *code;
};


int main()
{
    // read file
    FILE *f = fopen("../../../res.bin", "rb");
    if (f == NULL)
    {
        printf("Error: file doesn't exists\n");
        return 1;
    }
    
    BYTE *buf = malloc(1024 * 1024);
    int64_t len = fread(buf, 1, 1024 * 1024, f);
    fclose(f);
    
    // load worker
    int64_t res_len = 0;
    void *res = LoadWorker(buf, len, &res_len);
    if (res == NULL)
    {
        printf("Error: at loading file\n");
        return 1;
    }
    
    // print it
    for (int i = 0; i < res_len; ++i)
    {
        printf("%02X ", ((BYTE *)res)[i]);
    }
    printf("\n");

    // call it:
    {        
        printf("Running...\n");
        callExample(res);
    }

    {
        struct promise *p = (struct promise *)object_array[0];
        printf("First object is:\n");
        printf("set: %d\n", p->ready);
        for (int i = 0; i < 8; ++i)
        {
            printf("%02X ", ((BYTE *)p->data)[i]);
        }
        printf("\n");
    }

    return 0;
}
