#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"


int64_t next_local_id = 0;
HANDLE hIOCP;


struct connection_context
{
    OVERLAPPED overlapped;
    SOCKET socket;
    WSABUF wsaBuf;
    struct hive_connection *context;
    
    // to store current data - callback is called when buffer_len is received
    int64_t res_buffer_len;
    BYTE *res_buffer;
    BYTE buffer[4096];
};

struct hive_connection
{
    struct connection_context *ctx;
    int64_t local_id;
};


// TODO: remove 1024 as constant
struct hive_connection *connections[1024];


void HandleNewConnection(SOCKET client)
{
    struct hive_connection *new_connection = myMalloc(sizeof(*new_connection));
    struct connection_context *new_context = myMalloc(sizeof(*new_context));

    // initializate new connection

    new_connection->local_id = next_local_id++;
    connections[new_connection->local_id] = new_connection;
    
    new_connection->ctx = new_context;
    new_context->context = new_connection;

    CreateIoCompletionPort((HANDLE)client, hIOCP, (ULONG_PTR)client, 0);

    new_context->socket = client;
    new_context->wsaBuf.buf = (char *)new_context->buffer;
    new_context->wsaBuf.len = sizeof(new_context->buffer);

    DWORD flags = 0;
    WSARecv(client, &new_context->wsaBuf, 1, NULL, &flags, (OVERLAPPED *)new_context, NULL);
}


static DWORD ConnectionListnerWorker(void *param) 
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
        HandleNewConnection(client);
    }
    
    return 0;
}

#define API_REQUEST_MEM_PAGE 0x00
#define API_QUERY_OBJECT 0x02
#define API_RUN_WORKER 0x04

#define API_ANSWER_REQUEST_MEM_PAGE (API_REQUEST_MEM_PAGE | 0x1)
#define API_ANSWER_QUERY_OBJECT (API_QUERY_OBJECT | 0x1)

/* known calls */
/*

    object concept:
        address = 
            [40 bit page][24 bit value]

    message structure:
        1 byte: call type
        7 byte: body size [little endian]
        ----
        request data
    

    api CALLS:
        
        API_REQUEST_MEM_PAGE [broadcast]
            5 byte: memory page
        
        API_QUERY_OBJECT
            8 byte: object id
            8 byte: offset
            8 byte: size

        API_RUN_WORKER
            8 byte: worker id
            ---
            input table [without body table]

    api ANSWERS:
            
        API_ANSWER_REQUEST_MEM_PAGE
            // neibours
            8 byte: length of neibours
            [array
                16 byte: address
            ]
            // answer
            1 byte: Does this computer use requested page [0xFF / 0x00]

        API_ANSWER_QUERY_OBJECT
            8 byte: object id
            8 byte: offset
            8 byte: size
            ---- 
            raw bytes
*/


/*---------------------------------------------- receive api logic ---------------------------------------------*/

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
            myFree(ctx);
            continue;
        }

        print("Got message: %s\n", ctx->buffer);

        memset(ctx->buffer, 0, sizeof(ctx->buffer));
        
        DWORD flags = 0;
        WSARecv(ctx->socket, &ctx->wsaBuf, 1, NULL, &flags, &ctx->overlapped, NULL);
    }
    return 0;
}

/*---------------------------------------------- send api logic ---------------------------------------------*/


void sendMessage(const char *host, const char *port, const char *msg) 
{
    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    print("Message to [%s] [%s] > [%s]\n", host, port, msg);

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
        int64_t len = 0;
        while (msg[len]) len++;
        send(s, msg, len + 1, 0);
        closesocket(s);
    } 
    else 
    {
        print("Error: Could not connect to %s\n", host);
    }
}

void start_remote_subsystem() 
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    DWORD wkId;
    HANDLE hwk = CreateThread(NULL, 0, Worker, NULL, 0, &wkId);
    (void)hwk;

    
    int16_t port = 0;
    DWORD clId;
    HANDLE hcl = CreateThread(NULL, 0, ConnectionListnerWorker, &port, 0, &clId);
    (void)hcl;

    print("Messenger ready\n");
    
    while (1) 
    {
        print("Enter IP:Message\n");

        char ip[128], port[128], msg[128];

        myScanS(ip);
        myScanS(port);
        myScanS(msg);

        sendMessage(ip, port, msg);
    }
    
    WSACleanup();
}

