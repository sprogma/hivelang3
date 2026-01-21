#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"


_Atomic int64_t next_local_id = 0;
int64_t server_port = -1;
HANDLE hIOCP;


#define STATE_WAITING_MESSAGE -1
#define STATE_WAITING_BODY_SIZE_1 -2
#define STATE_WAITING_BODY_SIZE_2 -3
#define STATE_WAITING_BODY_SIZE_3 -4
#define STATE_WAITING_BODY_SIZE_4 -5
#define STATE_WAITING_BODY_SIZE_5 -6
#define STATE_WAITING_BODY_SIZE_6 -7
#define STATE_WAITING_BODY_SIZE_7 -8


// must Acquire Shared connection_lock
struct hive_connection *GetConnectionById(int64_t local_id, int64_t *index)
{
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id == local_id)
        {
            if (index)
            {
                *index = i;
            }
            return connections[i];
        }
    }
    return NULL;
}


#ifndef NDEBUG
void DumpConnections()
{
    AcquireSRWLockShared(&connections_lock);
    print("----- total %lld connections:\n", connections_len);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        print("-connection [%p] %lld: local_id=%lld\n", connections[i], i, connections[i]->local_id);
        if (connections[i]->outgoing == INVALID_SOCKET)
        {
            print("outgoing: No connection\n");
        }
        else
        {
            struct sockaddr_storage addr;
            int addrLen = sizeof(addr);

            if (getpeername(connections[i]->outgoing, (struct sockaddr*)&addr, &addrLen) == 0) 
            {
                char ipStr[INET6_ADDRSTRLEN];

                if (addr.ss_family == AF_INET)
                {
                    struct sockaddr_in* s = (struct sockaddr_in*)&addr;
                    inet_ntop(AF_INET, &s->sin_addr, ipStr, sizeof(ipStr));
                    print("outgoing: IP=%s PORT=%lld\n", ipStr, (int64_t)ntohs(s->sin_port));
                } 
                else if (addr.ss_family == AF_INET6) 
                {
                    struct sockaddr_in6* s = (struct sockaddr_in6*)&addr;
                    inet_ntop(AF_INET6, &s->sin6_addr, ipStr, sizeof(ipStr));
                    print("outgoing:  IP=%s PORT=%lld\n", ipStr, (int64_t)ntohs(s->sin6_port));
                }
                else
                {
                    print("outgoing:  Corrupted\n");
                }
            }
        }
        if (connections[i]->ctx == NULL)
        {
            print("incoming: No connection\n");
        }
        else
        {
            struct sockaddr_storage addr;
            int addrLen = sizeof(addr);

            if (getpeername(connections[i]->ctx->socket, (struct sockaddr*)&addr, &addrLen) == 0) 
            {
                char ipStr[INET6_ADDRSTRLEN];

                if (addr.ss_family == AF_INET)
                {
                    struct sockaddr_in* s = (struct sockaddr_in*)&addr;
                    inet_ntop(AF_INET, &s->sin_addr, ipStr, sizeof(ipStr));
                    print("incoming: IP=%s PORT=%lld\n", ipStr, (int64_t)ntohs(s->sin_port));
                } 
                else if (addr.ss_family == AF_INET6) 
                {
                    struct sockaddr_in6* s = (struct sockaddr_in6*)&addr;
                    inet_ntop(AF_INET6, &s->sin6_addr, ipStr, sizeof(ipStr));
                    print("incoming:  IP=%s PORT=%lld\n", ipStr, (int64_t)ntohs(s->sin6_port));
                }
                else
                {
                    print("outgoing:  Corrupted\n");
                }
            }
        }
    }
    ReleaseSRWLockShared(&connections_lock);
}
#else
#define DumpConnections()
#endif


void SendPageAllocationConfirm(struct hive_connection *con, BYTE *broadcast_id);
void ConfirmConnection(struct hive_connection *con, int64_t local_id, int64_t port);
int64_t RedirectBroadcastQuery(int64_t page_id, BYTE *broadcast_id, int64_t except_this_local_id);


void ConfirmPage(int64_t page_id)
{
    log("!!!!! >>>>>> Page allocation id=%lld confirmed\n", page_id);
    
    AcquireSRWLockExclusive(&pages_lock);
    pages[pages_len++] = (struct memory_page){page_id, 0};
    ReleaseSRWLockExclusive(&pages_lock);
}


void HandleNewConnection(SOCKET client, SOCKADDR_STORAGE storage, int storage_len)
{
    log("Client connected\n");
    
    struct hive_connection *new_connection = myMalloc(sizeof(*new_connection));
    struct connection_context *new_context = myMalloc(sizeof(*new_context));

    // initializate new connection
    new_connection->outgoing = INVALID_SOCKET;
    new_connection->local_id = next_local_id++;
    new_connection->address = storage;
    new_connection->address_len = storage_len;
    new_connection->ctx = new_context;

    AcquireSRWLockExclusive(&connections_lock);
    connections[connections_len++] = new_connection;
    ReleaseSRWLockExclusive(&connections_lock);
    
    new_context->connection = new_connection;
    new_context->res_buffer_len = STATE_WAITING_MESSAGE;
    new_context->res_buffer = myMalloc(4096);
    
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
    
    log("Server started...\n");
    
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
            server_port = boundAddr.sin6_port;
            print("server started on port %lld\n", (int64_t)ntohs(boundAddr.sin6_port));
        }
    }
    
    listen(listenSock, SOMAXCONN);
    
    while (1)
    {
        SOCKADDR_STORAGE storage;
        int storage_len = sizeof(storage);
        SOCKET client = accept(listenSock, (SOCKADDR *)&storage, &storage_len);
        if (client != INVALID_SOCKET)
        {
            HandleNewConnection(client, storage, storage_len);
        }
    }
    
    return 0;
}

#define API_REQUEST_CONNECTION 0x00
#define API_REQUEST_MEM_PAGE 0x02
#define API_QUERY_OBJECT 0x04

#define API_ANSWER_REQUEST_CONNECTION (API_REQUEST_CONNECTION | 0x1)
#define API_ANSWER_REQUEST_MEM_PAGE (API_REQUEST_MEM_PAGE | 0x1)
#define API_ANSWER_QUERY_OBJECT (API_QUERY_OBJECT | 0x1)

#define BROADCAST_ID_LENGTH 27

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

        API_REQUEST_CONNECTION
            8 byte: reply_id
        
        API_REQUEST_MEM_PAGE [broadcast]
            5 byte: memory page
            27 byte: broadcast ID
        
        API_QUERY_OBJECT
            8 byte: object_id
            8 byte: offset
            8 byte: size

    api ANSWERS:

        API_ANSWER_REQUEST_CONNECTION:
            8 byte: reply_id
            
        API_ANSWER_REQUEST_MEM_PAGE
            27 byte: broadcast ID

        API_ANSWER_QUERY_OBJECT
            8 byte: object_id
            8 byte: offset
            8 byte: size
            16+2 byte: ip+port [if port == 0, then path can't be optimized]
            ---- 
            raw bytes
*/


/*---------------------------------------------- receive api logic ---------------------------------------------*/
static int64_t HandleApiCall(struct hive_connection *con)
{
    struct connection_context *ctx = con->ctx;
    print("Get api call %lld of length: %lld\n", ctx->res_api_call, ctx->res_buffer_len);
    switch (ctx->res_api_call)
    {
        case API_REQUEST_CONNECTION:
        {
            // simply reply to same host
            int64_t reply_id = *(int64_t *)ctx->res_buffer;
            int64_t port = *(int64_t *)(ctx->res_buffer + 8);
            ConfirmConnection(con, reply_id, port);
            log("API_REQUEST_CONNECTION answered\n");
            return 1;
        }
        case API_ANSWER_REQUEST_CONNECTION:
        {
            // update that id
            AcquireSRWLockShared(&connections_lock);
            int64_t local_id = *(int64_t *)ctx->res_buffer;
            for (int64_t i = 0; i < connections_len; ++i)
            {
                if (connections[i]->local_id == local_id)
                {
                    con->outgoing = connections[i]->outgoing;
                    log("API_ANSWER_REQUEST_CONNECTION updated using local_id=%lld\n", local_id);
                    
                    int64_t index;
                    
                    struct hive_connection *old_con = GetConnectionById(local_id, &index);
                    myFree(old_con);
                    connections[index] = connections[--connections_len];
                    ReleaseSRWLockShared(&connections_lock);
                    return 1;
                }
            }
            print("Error: API_ANSWER_REQUEST_CONNECTION becouse there is no connection with local_id=%lld\n", local_id);
            ReleaseSRWLockShared(&connections_lock);
            return 0;
        }
        case API_REQUEST_MEM_PAGE:
        {
            int64_t page_id;
            memcpy(&page_id, ctx->res_buffer, 5);
            BYTE *broadcast_id = ctx->res_buffer + 5;
            
            log("API_REQUEST_MEM_PAGE page=%lld [prefix=%llx]\n", page_id, *(int64_t *)broadcast_id);
            // check - is page used?
            AcquireSRWLockShared(&pages_lock);
            for (int64_t i = 0; i < pages_len; ++i)
            {
                if (pages[i].id == page_id)
                {
                    // refuse
                    log("Refuse allocation\n");
                    ReleaseSRWLockShared(&pages_lock);
                    return 1;
                }
            }
            ReleaseSRWLockShared(&pages_lock);

            // check - if any other broadcast waiting for this page
            // TODO: maybe use Shared access here, and only if page isn't requested - use exclusive?
            AcquireSRWLockExclusive(&known_broadcasts.lock);
            
            for (int64_t i = 0; i < known_broadcasts.alloc; ++i)
            {
                struct hashtable_node *cur = known_broadcasts.table[i];
                while (cur != NULL)
                {
                    if (!equal_bytes(cur->bytes, broadcast_id, BROADCAST_ID_LENGTH) &&
                        ((struct memory_page_request *)cur->id)->page_id == page_id)
                    {
                        // refuse
                        log("Refuse allocation\n");
                        ReleaseSRWLockExclusive(&known_broadcasts.lock);
                        return 1;
                    }
                    cur = cur->next;
                }
            }
            
            // check - if we already answering this broadcast
            struct memory_page_request *broadcast = (struct memory_page_request *)GetHashtableNoLock(&known_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 0);
            if (broadcast == NULL)
            {
                // create local broadcast entry
                broadcast = myMalloc(sizeof(*broadcast));
                broadcast->page_id = page_id;
                broadcast->local_redirect_id = con->local_id;
                broadcast->answered = 0;
                broadcast->requested = 0;
                SetHashtableNoLock(&known_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, (int64_t)broadcast);

                // redirect queries
                broadcast->requested = RedirectBroadcastQuery(page_id, broadcast_id, con->local_id);
            }
            else
            {
                if (broadcast->local_redirect_id != con->local_id)
                {
                    print("ASSERT ERROR: broadcast->local_redirect_id != con->local_id [line %lld]\n", (int64_t)__LINE__);
                }
            }

            int64_t answered = broadcast->answered;
            int64_t requested = broadcast->requested;
            ReleaseSRWLockExclusive(&known_broadcasts.lock);

            // is broadcast accepted by all neibours?
            if (answered == requested)
            {
                SendPageAllocationConfirm(con, broadcast_id);
                return 1;
            }

            // else - waiting for more acceptions
            return 1;
        }
        case API_ANSWER_REQUEST_MEM_PAGE:
        {
            BYTE *broadcast_id = ctx->res_buffer;
            // get broadcast
            log("Get broadcast answer [prefix=%llx]\n", *(int64_t *)broadcast_id);
            AcquireSRWLockShared(&known_broadcasts.lock);
            struct memory_page_request *broadcast = (struct memory_page_request *)GetHashtableNoLock(&known_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 0);
            if (broadcast != NULL)
            {
                int64_t new_count = ++broadcast->answered;
                if (new_count == broadcast->requested) // answer query
                {
                    log("Broadcast confirmed\n");
                    if (broadcast->local_redirect_id == -1)
                    {
                        // request confirmed
                        ConfirmPage(broadcast->page_id);
                    }
                    else
                    {
                        AcquireSRWLockShared(&connections_lock);
                        struct hive_connection *ansCon = GetConnectionById(broadcast->local_redirect_id, NULL);
                        ReleaseSRWLockShared(&connections_lock);
                        if (ansCon)
                        {
                            SendPageAllocationConfirm(ansCon, broadcast_id);
                        }
                    }
                }
            }
            ReleaseSRWLockShared(&known_broadcasts.lock);
            return 1;
        }
        case API_QUERY_OBJECT:
        {
            // int64_t object_id = *(int64_t *)(ctx->res_buffer);
            // int64_t query_offset = *(int64_t *)(ctx->res_buffer+8);
            // int64_t query_size = *(int64_t *)(ctx->res_buffer+16);

            /*
                test variants:

                1. object can be not on this hive
                2. object can be not ready [promise/pipe]
                3. object can be ready
            */
        }
    }
    return 0;
}

static DWORD Worker(void *param)
{
    (void)param;
    
    DWORD bytesReceived, bytesProcessed;
    ULONG_PTR completionKey;
    LPOVERLAPPED lpOverlapped;

    while (GetQueuedCompletionStatus(hIOCP, &bytesReceived, &completionKey, &lpOverlapped, INFINITE)) 
    {
        struct connection_context *ctx = (struct connection_context *)lpOverlapped;

        if (bytesReceived == 0)
        {
            log("[NETWORK]: Peer disconnected\n");
            AcquireSRWLockExclusive(&connections_lock);
            int64_t index;
            GetConnectionById(ctx->connection->local_id, &index);
            connections[index] = connections[--connections_len];
            ReleaseSRWLockShared(&connections_lock);

            if (ctx->connection->outgoing != INVALID_SOCKET)
            {
                closesocket(ctx->connection->outgoing);
            }
            closesocket(ctx->socket);
            myFree(ctx);
            myFree(ctx->res_buffer);
            myFree(ctx->connection);
            continue;
        }

        log("Get message of %lld bytes\n", bytesReceived);

        int deleteConnection = 0;

        bytesProcessed = 0;
        /* receive header */
        while (bytesProcessed < bytesReceived)
        {
            if (ctx->res_buffer_len == STATE_WAITING_MESSAGE)
            {
                ctx->res_api_call = ctx->buffer[bytesProcessed++];
                ctx->res_buffer_len = STATE_WAITING_BODY_SIZE_1;
                log("process header[ok] [header=%lld]\n", ctx->res_api_call);
            }
            else if (ctx->res_buffer_len < 0)
            {
                // one of STATE_WAITING_BODY_SIZE_X
                DWORD readBytes = bytesReceived - bytesProcessed;
                int64_t current_body_size = -2-ctx->res_buffer_len;
                int64_t missing_body_size = 7 - current_body_size;
                if (readBytes >= missing_body_size)
                {
                    ctx->res_buffer_len = 0;
                    ctx->res_buffer_current_len = 0;
                    memcpy(&ctx->res_buffer_len, &ctx->res_buffer[0], current_body_size);
                    memcpy(((BYTE *)&ctx->res_buffer_len) + current_body_size, &ctx->buffer[bytesProcessed], missing_body_size);
                    bytesProcessed += missing_body_size;
                    log("process length[ok] [length=%lld]\n", ctx->res_buffer_len);
                }
                else
                {
                    memcpy(&ctx->res_buffer[current_body_size], &ctx->buffer[bytesProcessed], readBytes);
                    ctx->res_buffer_len = -2-(current_body_size + readBytes);
                    bytesProcessed += readBytes;
                    log("process length[part]\n");
                }
            }
            else
            {
                DWORD readBytes = bytesReceived - bytesProcessed;
                int64_t current_body_size = ctx->res_buffer_current_len;
                int64_t missing_body_size = ctx->res_buffer_len - current_body_size;
                if (readBytes >= missing_body_size)
                {
                    memcpy(&ctx->res_buffer[current_body_size], &ctx->buffer[bytesProcessed], missing_body_size);
                    ctx->res_buffer_current_len = ctx->res_buffer_len;
                    bytesProcessed += missing_body_size;
                    // run api callback
                    log("process body[ok]\n");
                    if (!HandleApiCall(ctx->connection))
                    {
                        log("closing connection...\n");
                        // delete this connection.
                        deleteConnection = 1;
                        closesocket(ctx->socket);
                        if (ctx->connection->outgoing != INVALID_SOCKET)
                        {
                            closesocket(ctx->connection->outgoing);
                        }
                        myFree(ctx->connection);
                        myFree(ctx->res_buffer);
                        myFree(ctx);
                        break;
                    }
                    ctx->res_buffer_len = STATE_WAITING_MESSAGE;
                }
                else
                {
                    memcpy(&ctx->res_buffer[current_body_size], &ctx->buffer[bytesProcessed], readBytes);
                    ctx->res_buffer_current_len += readBytes;
                    bytesProcessed += readBytes;
                    log("process body[part]\n");
                }
            }
        }

        if (!deleteConnection)
        {
            DWORD flags = 0;
            WSARecv(ctx->socket, &ctx->wsaBuf, 1, NULL, &flags, &ctx->overlapped, NULL);
        }
    }
    return 0;
}

/*---------------------------------------------- send api logic ---------------------------------------------*/


void SendPageAllocationConfirm(struct hive_connection *con, BYTE *broadcast_id)
{
    log("Send confirmation of page allocation to local_id=%lld\n", con->local_id);
    BYTE message[8+27] = {API_ANSWER_REQUEST_MEM_PAGE, BROADCAST_ID_LENGTH, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8, broadcast_id, BROADCAST_ID_LENGTH);
    send(con->outgoing, (char *)message, sizeof(message), 0);
}


int64_t RedirectBroadcastQuery(int64_t page_id, BYTE *broadcast_id, int64_t except_this_local_id)
{
    AcquireSRWLockShared(&connections_lock);
    BYTE message[8+5+27] = {API_REQUEST_MEM_PAGE, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8,  &page_id, 5);
    memcpy(message + 8+5, broadcast_id, BROADCAST_ID_LENGTH);
    int64_t send_cnt = 0;
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id != except_this_local_id)
        {
            log("Redirecting query to local_id=%lld\n", connections[i]->local_id);
            send(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            send_cnt++;
        }
    }
    ReleaseSRWLockShared(&connections_lock);
    return send_cnt;
}


void ConfirmConnection(struct hive_connection *ctx, int64_t local_id, int64_t port)
{
    SOCKET s = INVALID_SOCKET;

    int family = ((SOCKADDR *)(&ctx->address))->sa_family;
    int socktype = SOCK_STREAM;
    int protocol = IPPROTO_TCP;

    s = socket(family, socktype, protocol);
    if (s == INVALID_SOCKET) 
    {
        log("socket() failed [error=%lld]\n", (int64_t)GetLastError());
        return;
    }

    
    if (ctx->address.ss_family == AF_INET) 
    {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)&ctx->address;
        ipv4->sin_port = port;
    } 
    else if (ctx->address.ss_family == AF_INET6) 
    {
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)&ctx->address;
        ipv6->sin6_port = port;
    }

    log("Requesting port %lld\n", port);

    if (connect(s, (SOCKADDR *)&ctx->address, ctx->address_len) != 0) 
    {
        log("connect failed [error=%lld]\n", (int64_t)GetLastError());
        closesocket(s);
        s = INVALID_SOCKET;
    } 
    else 
    {
        log("Connection is successful!\n");
        AcquireSRWLockShared(&connections_lock);
        connections[local_id]->outgoing = s;
        ReleaseSRWLockShared(&connections_lock);

        BYTE header[8] = {0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send(s, (char *) header, sizeof(header), 0);
        send(s, (char *)&local_id, 8, 0);
    }
}

int64_t InitiateConnection(const char *host, const char *port)
{
    if (server_port == -1)
    {
        log("Error - server_port == -1 [server isn't started]\n");
    }

    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    log("Starting new connection to [%s] [%s]\n", host, port);
    
    int status = getaddrinfo(host, port, &hints, &result);
    if (status != 0)
    {
        log("Error resolving host. [status=%lld] = %s\n", (int64_t)status, gai_strerror(status));
        return -1;
    }
    
    SOCKET s = INVALID_SOCKET;
    for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) 
    {
        s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (s == INVALID_SOCKET) continue;

        if (connect(s, ptr->ai_addr, ptr->ai_addrlen) == 0) 
        {
            break;
        }

        closesocket(s);
        s = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (s != INVALID_SOCKET) 
    {
        log("Connection is successful!\n");
        
        AcquireSRWLockExclusive(&connections_lock);
        
        int64_t local_id = next_local_id++;
        int64_t conn = connections_len++;
        
        connections[conn] = myMalloc(sizeof(**connections));
        connections[conn]->ctx = NULL;
        connections[conn]->outgoing = s;
        connections[conn]->local_id = local_id;
        
        ReleaseSRWLockExclusive(&connections_lock);
        
        BYTE header[8] = {API_REQUEST_CONNECTION, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send(s, (char *) header, sizeof(header), 0);
        send(s, (char *)&local_id, 8, 0);
        send(s, (char *)&server_port, 8, 0);
        return local_id;
    } 
    else 
    {
        print("Error: Could not connect to %s\n", host);
        return -1;
    }
}


void RequestMemoryPage(int64_t page_id)
{
    log("Trying to get memory page %lld\n", page_id);


    // create random seed
    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    BCryptGenRandom(NULL, broadcast_id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    
    AcquireSRWLockExclusive(&known_broadcasts.lock);

    // request page from all neibours
    struct memory_page_request *broadcast = myMalloc(sizeof(*broadcast));
    broadcast->page_id = page_id;
    broadcast->local_redirect_id = -1; // this hive
    broadcast->answered = 0;
    broadcast->requested = 0;
    SetHashtableNoLock(&known_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, (int64_t)broadcast);
    
    log("Created broadcast with prefix=%llx\n", *(int64_t *)broadcast_id);

    // redirect queries
    broadcast->requested = RedirectBroadcastQuery(page_id, broadcast_id, -1);

    int64_t requested = broadcast->requested;
    ReleaseSRWLockExclusive(&known_broadcasts.lock);

    if (requested == 0)
    {
        // confirm page alloaction
        ConfirmPage(page_id);
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

    
//     while (1) 
//     {
//         char cmd[128];
//         myScanS(cmd);
//         if (cmd[0] == 'c' || cmd[0] == 'C')
//         {
//             print("Selected connect command.\n");
//             print("Enter IP and PORT.\n");
//             
//             char host[128];
//             char port[128];
//             myScanS(host);
//             myScanS(port);
//             print("Trying to connect to [%s] [%s]...\n", host, port);
//             int64_t res = InitiateConnection(host, port);
//             print("Temporary local_id = %lld\n", res);
//         }
//         else if (cmd[0] == 'm' || cmd[0] == 'M')
//         {
//             print("Selected request memory page.\n");
//             print("Enter memory page value.\n");
// 
//             int64_t page = myScanI64();
// 
//             RequestMemoryPage(page);
//         }
//         else if (cmd[0] == 'd' || cmd[0] == 'D')
//         {
//             DumpConnections();
//         }
//     }
    
    WSACleanup();
}

