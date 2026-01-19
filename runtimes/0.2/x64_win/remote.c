#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

// #include "runtime_lib.h"

#include "stdio.h"
#include "inttypes.h"

#define print printf
#define log printf


struct connection_context
{
    OVERLAPPED overlapped;
    SOCKET socket;
    WSABUF wsaBuf;
    char buffer[1024];
};


HANDLE hIOCP;


static DWORD Worker(void *param)
{
    (void)param;
    
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED lpOverlapped;

    while (GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &completionKey, &lpOverlapped, INFINITE)) 
    {
        struct connection_context *ctx = (struct connection_context *)lpOverlapped;

        if (bytesTransferred == 0) 
        {
            log("[NETWORK]: Peer disconnected\n");
            closesocket(ctx->socket);
            free(ctx);
            continue;
        }

        print("Got message: %s\n", ctx->buffer);

        memset(ctx->buffer, 0, sizeof(ctx->buffer));
        
        DWORD flags = 0;
        WSARecv(ctx->socket, &ctx->wsaBuf, 1, NULL, &flags, &ctx->overlapped, NULL);
    }
    return 0;
}

static DWORD ConnectionListener(void *param) 
{
    (void)param;
    
    SOCKET listenSock = socket(AF_INET6, SOCK_STREAM, 0);
    
    int32_t no = 0;
    setsockopt(listenSock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&no, sizeof(no));

    struct sockaddr_in6 addr;
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(*(int16_t *)param);
    addr.sin6_addr = in6addr_any;
    
    if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) == 0) 
    {
        struct sockaddr_in6 boundAddr;
        int addrLen = sizeof(boundAddr);

        if (getsockname(listenSock, (struct sockaddr*)&boundAddr, &addrLen) == 0) {
            print("server started on port %lld\n", (int64_t)ntohs(boundAddr.sin6_port));
        }
    }
    
    listen(listenSock, SOMAXCONN);
    
    while (1) 
    {
        SOCKET client = accept(listenSock, NULL, NULL);
        CreateIoCompletionPort((HANDLE)client, hIOCP, (ULONG_PTR)client, 0);

        struct connection_context* ctx = malloc(sizeof(*ctx));
        ctx->socket = client;
        ctx->wsaBuf.buf = ctx->buffer;
        ctx->wsaBuf.len = 1024;

        DWORD flags = 0;
        WSARecv(client, &ctx->wsaBuf, 1, NULL, &flags, (OVERLAPPED *)ctx, NULL);
    }
    
    return 0;
}

// void start_remote_subsystem()
// {
// }

void sendMessage(const char *host, const char *port, const char *msg) 
{
    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    print("Message to %s %s %s\n", host, port, msg);

    int status = getaddrinfo(host, port, &hints, &result);
    if (status != 0) 
    {
        print("Error resolving host. [status=%lld] = %s\n", (int64_t)status, gai_strerror(status));
        return; 
    }

    SOCKET s = INVALID_SOCKET;
    for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) 
    {
        s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (s == INVALID_SOCKET) continue;

        if (connect(s, ptr->ai_addr, (int)ptr->ai_addrlen) == 0) 
        {
            break;
        }

        closesocket(s);
        s = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (s != INVALID_SOCKET) 
    {
        send(s, msg, strlen(msg) + 1, 0);
        closesocket(s);
    } 
    else 
    {
        print("Error: Could not connect to %s\n", host);
    }
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    DWORD wkId;
    HANDLE hwk = CreateThread(NULL, 0, Worker, NULL, 0, &wkId);
    (void)hwk;

    
    int16_t port = 0;
    DWORD clId;
    HANDLE hcl = CreateThread(NULL, 0, ConnectionListener, &port, 0, &clId);
    (void)hcl;

    print("Messenger ready\n");
    
    while (1) 
    {
        print("Enter IP:Message\n");

        char ip[128], port[128], msg[128];
        scanf("%s%s%s", ip, port, msg);

        sendMessage(ip, port, msg);
    }
    
    WSACleanup();
    return 0;
}
