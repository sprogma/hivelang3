#include "inttypes.h"
#include "stdio.h"

extern int MyAsmFunc(void);

void *objects[1000];


// if dest == NULL - return into rax
int64_t QueryObject(void *dest, int64_t object, int64_t offset, int64_t size)
{
    printf("CALL QUERY OBJECT WITH %p <- %lld[%lld:%lld]\n", dest, object, offset, size);
    return 0;
}

// if size < 0 -> src is constant value [int64_t]
void PushObject(int64_t object, int64_t offset, int64_t size, void *src)
{
    printf("CALL PUSH OBJECT WITH %lld[%lld:%lld] <- %p\n", object, offset, size, src);
}

// size is size of element in pipe
#define NEW_OBJECT_PIPE    0x01

// size is size of element in promise
#define NEW_OBJECT_PROMISE 0x02

// size is total size of object
#define NEW_OBJECT_OBJECT  0x03

// return new object [id] in rax
int64_t NewObject(int64_t type, int64_t size)
{
    printf("CALL NEW OBJECT WITH rax <- new %lld with param %lld\n", type, size);
    return 0;
}

void CallObject(int64_t worker)
{
    printf("CALL OF WORKER %lld WITH args.\n", worker);
}

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



int main()
{
    printf("%d\n", MyAsmFunc());
}
