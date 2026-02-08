#ifndef PROVIDERS_H
#define PROVIDERS_H


#include "inttypes.h"
#include "runtime.h"


//<<--Quote-->> from:../../../codegen/codegen.hpp:.*ProviderId.*\n?\{(?>[^{}]+|(?<o>\{)|(?<-o>\}))+(?(o)(?!))\}
// static inline int64_t ProviderId(const string &name)
// {
//     if (name == "x64")
//     {
//         return 0;
//     }
//     if (name == "gpu")
//     {
//         return 1;
//     }
//     return -1;
// }
//<<--QuoteEnd-->>

enum
{
    PROVIDER_X64=0,
    PROVIDER_GPU=1,
};

void universalPauseWorker(void *returnAddress, void *rbpValue, enum worker_wait_state state, void *state_data);
void universalUpdateLocalPush(void *obj, int64_t offset, int64_t size, void *source);


#endif
