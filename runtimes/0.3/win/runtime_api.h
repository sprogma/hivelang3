#ifndef RUNTIME_API_H
#define RUNTIME_API_H


#ifndef _WIN32_WINNT
    #define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"


#include "runtime_lib.h"
#include "remote.h"


// fn(rdi, rsi, rdx, rcx, returnAddress, rbpValue)


//<<--Quote-->> from:../../../codegen/x64_win/codegen.cpp:(//\s*(rdi|rsi|rdx|rcx)=.*|case OP_\w*(CALL|CAST|NEW|PUSH|QUERY)\w*:)
// case OP_CALL:
// // rdi="on" parameter
// // rsi=call table
// // rdx=worker id
// case OP_CAST:
// // rdi=object
// // rsi=toID | (object_type << 8)
// // rdx=fromID | (object_type << 8)
// // rcx=objectSize
// case OP_NEW_INT:
// case OP_NEW_FLOAT:
// case OP_NEW_STRING:
// // rdi=OBJECT_DEFINED_ARRAY=5
// // rsi=defined object ID
// // rdx=size of element
// case OP_NEW_ARRAY:
// // rdi=OBJECT_ARRAY=3
// // rsi=total size
// // rdx=size of element
// case OP_NEW_PROMISE:
// // rdi=OBJECT_PROMISE=2
// // rsi=total size
// // rdx=size of element = total size
// case OP_NEW_CLASS:
// // rdi=OBJECT_OBJECT=4
// // rsi=total size
// // rdx=size of element = total size
// case OP_NEW_PIPE:
// // rdi=OBJECT_PIPE=1
// // rsi=total size
// // rdx=size of element = total size
// case OP_PUSH_VAR:
// case OP_PUSH_ARRAY:
// // rcx=size rdx=offset rdi=object rsi=value
// case OP_PUSH_PROMISE:
// // rcx=size rdx=offset rdi=object rsi=value
// case OP_PUSH_PIPE:
// // rcx=size rdx=offset rdi=object rsi=value
// case OP_PUSH_CLASS:
// // rcx=size rdx=offset rdi=object rsi=value
// case OP_QUERY_VAR:
// case OP_QUERY_INDEX:
// // rcx=size rdx=offset rdi=value rsi=object
// case OP_QUERY_ARRAY:
// // rcx=size rdx=offset rdi=value rsi=object
// case OP_QUERY_PROMISE:
// // rcx=size rdx=offset rdi=value rsi=object
// case OP_QUERY_CLASS:
// // rcx=size rdx=offset rdi=value rsi=object
// case OP_QUERY_PIPE:
// // rcx=size rdx=offset rdi=value rsi=object
//<<--QuoteEnd-->>


extern int x64_fastPushObject(void);
extern int x64_fastQueryObject(void);
extern int x64_fastNewObject(void);
extern int x64_fastCallObject(void);
extern int x64_fastPushPipe(void);
extern int x64_fastQueryPipe(void);
extern int gpu_fastNewObject(void);
extern int gpu_fastCallObject(void);
extern int loc_fastNewObject(void);
extern int dll_fastCallObject(void);
extern int any_fastCastProvider(void);


// fn(rdi, rsi, rdx, rcx, returnAddress, rbpValue)


__attribute__((sysv_abi))
int64_t x64QueryObject  (void   *destination, int64_t object,    int64_t offset,    int64_t size,            void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
int64_t x64QueryPipe    (void   *destination, int64_t object_id, int64_t offset,    int64_t size,            void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
void    x64PushObject   (int64_t object,      void   *source,    int64_t offset,    int64_t size,            void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
void    x64PushPipe     (int64_t object,      void   *source,    int64_t offset,    int64_t size,            void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
int64_t x64NewObject    (int64_t type,        int64_t size,      int64_t param,     int64_t _,               void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
int64_t gpuNewObject    (int64_t type,        int64_t size,      int64_t param,     int64_t _,               void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
int64_t locNewObject    (int64_t type,        int64_t size,      int64_t param,     int64_t _,               void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
void    x64CallObject   (int64_t moditifer,   BYTE *args,        int64_t workerId,  int64_t _,               void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
void    gpuCallObject   (int64_t moditifer,   BYTE *args,        int64_t workerId,  int64_t _,               void *returnAddress, void *rbpValue);
__attribute__((sysv_abi))
int64_t anyCastProvider (void *obj,           int64_t to,        int64_t from,      int64_t object_size,     void *returnAddress, void *rbpValue);

#endif
