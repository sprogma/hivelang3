#ifndef LOC_PROVIDER_H
#define LOC_PROVIDER_H


#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"


#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"


void locNewObjectUsingPage(int64_t type, int64_t size, int64_t param, int64_t *remote_id);

#endif
