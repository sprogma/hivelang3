#include "system.h"

#include "runtime_lib.h"
#include "runtime.h"
#include "remote.h"
#include "providers.h"

lock_t ServerIdGetLock = INIT_LOCK;

_Atomic int64_t next_local_id = 0;
int64_t server_port = -1;
_Atomic int64_t thisServerId = -1;
_Atomic int64_t glbStatRemotePathMisses = 0;
_Atomic int64_t glbStatRemoteOutputRequests = 0;
_Atomic int64_t glbStatRemoteInputRequests = 0;
#ifdef _WIN32
HANDLE hIOCP;
#else
int hEpoll;
#endif


#define STATE_WAITING_MESSAGE -1
#define STATE_WAITING_BODY_SIZE_1 -2
#define STATE_WAITING_BODY_SIZE_2 -3
#define STATE_WAITING_BODY_SIZE_3 -4
#define STATE_WAITING_BODY_SIZE_4 -5
#define STATE_WAITING_BODY_SIZE_5 -6
#define STATE_WAITING_BODY_SIZE_6 -7
#define STATE_WAITING_BODY_SIZE_7 -8


int b_send(struct bufferized_socket *s, const char *msg, int len, int flags) 
{
    (void)flags;
    while (len > 0) 
    {
        int c = (int)((len < (int)((int64_t)sizeof(s->buffer) - s->buffer_len)) ? len : (int64_t)sizeof(s->buffer) - s->buffer_len);
        memcpy(s->buffer + s->buffer_len, msg, c);
        s->buffer_len += c; 
        msg += c; 
        len -= c;
        if (s->buffer_len == sizeof(s->buffer)) 
        {
            if (send(s->sock, (char *)s->buffer, s->buffer_len, 0) <= 0) 
            { 
                return -1; 
            }
            s->buffer_len = 0;
        }
    }
    return 0;
}

void b_flush(struct bufferized_socket *s) {
    if (s->buffer_len > 0) 
    { 
        send(s->sock, (char *)s->buffer, s->buffer_len, 0); 
        s->buffer_len = 0; 
    }
}

// #define BUFERIZATE 1 // eats whole cpu kernel
#define BUFERIZATE 0
#define NODELAY_MODE 1

#if BUFERIZATE == 1
    #define emitData(ssock, ...) b_send(&ssock, __VA_ARGS__)
    #define timedFlush(skt) b_flush(&skt)
#else
    #define emitData(ssock, ...) send((ssock).sock, __VA_ARGS__)
    #define timedFlush(skt) 
#endif


// must Acquire Shared connection_lock
struct hive_connection *GetConnectionById(int64_t local_id, int64_t *index)
{
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->local_id == local_id)
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


#if !defined(NDEBUG) && defined(_WIN32)
void DumpConnections()
{
    lock_read(&connections_lock);
    print("----- total %lld connections:\n", connections_len);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        print("-connection [%p] %lld: local_id=%lld\n", connections[i], i, connections[i]->local_id);
        if (connections[i]->outgoing.sock == INVALID_SOCKET)
        {
            print("outgoing: No connection\n");
        }
        else
        {
            struct sockaddr_storage addr;
            int addrLen = sizeof(addr);

            if (getpeername(connections[i]->outgoing.sock, (struct sockaddr*)&addr, &addrLen) == 0) 
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
    unlock_read(&connections_lock);
}
#else
#define DumpConnections()
#endif



void ConfirmPage(int64_t page_id)
{
    log("!!!!! >>>>>> Page allocation id=%lld confirmed\n", page_id);
    
    lock_write(&pages_lock);
    pages[pages_len++] = (struct memory_page){page_id, 0, 0};
    unlock_write(&pages_lock);
}

void ConfirmID(int64_t confirmed_id)
{
    if (thisServerId == -1)
    {
        print("Server id=%lld confirmed\n", confirmed_id);
        thisServerId = confirmed_id;
        unlock_write(&ServerIdGetLock);
    }
}


#ifndef _WIN32
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-octal-literals"
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    #pragma clang diagnostic pop
}
#endif

#ifdef _WIN32
void HandleNewConnection(socket_t client, SOCKADDR_STORAGE storage, int storage_len)
#else
void HandleNewConnection(socket_t client, struct sockaddr_storage storage, int storage_len)
#endif
{
    print("Client connected!\n");
    
    struct hive_connection *new_connection = myMalloc(sizeof(*new_connection));
    struct connection_context *new_context = myMalloc(sizeof(*new_context));

    // initializate new connection
    new_connection->lock = (lock_t)INIT_LOCK;
    new_connection->outgoing = (struct bufferized_socket){INVALID_SOCKET, 0, {}};
    new_connection->local_id = next_local_id++;
    new_connection->address = storage;
    new_connection->address_len = storage_len;
    new_connection->ctx = new_context;
    new_connection->wait_list_len = INT_INFINITY;
    new_connection->queue_len = INT_INFINITY;
    new_connection->idle_time = 0;

    lock_write(&connections_lock);
    connections[connections_len++] = new_connection;
    unlock_write(&connections_lock);
    
    new_context->connection = new_connection;
    new_context->res_buffer_len = STATE_WAITING_MESSAGE;
    new_context->res_buffer = myMalloc(4096);

    #ifdef _WIN32
    CreateIoCompletionPort((HANDLE)client, hIOCP, (ULONG_PTR)client, 0);
    
    new_context->socket = client;
    new_context->wsaBuf.buf = (char *)new_context->buffer;
    new_context->wsaBuf.len = sizeof(new_context->buffer);

    DWORD flags = 0;
    WSARecv(client, &new_context->wsaBuf, 1, NULL, &flags, (OVERLAPPED *)new_context, NULL);
    #else
    set_nonblocking(client);
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = new_context;

    if (epoll_ctl(hEpoll, EPOLL_CTL_ADD, client, &ev) == -1) {
        perror("epoll_ctl: client");
        exit(1);
    }
    #endif
}


#ifdef _WIN32
static DWORD ConnectionListnerWorker(void *param) 
#else
static void *ConnectionListnerWorker(void *param) 
#endif
{
    (void)param;
    
    log("Server started...\n");
    
    socket_t listenSock = socket(AF_INET6, SOCK_STREAM, 0);
    
    int32_t no = 0;
    setsockopt(listenSock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&no, sizeof(no));

    #ifndef _WIN32
    int reuse = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    #endif

    struct sockaddr_in6 addr = {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(*(int16_t *)param);
    addr.sin6_addr = in6addr_any;

    log("binding...\n");
    
    if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) == 0) 
    {
        log("Socket created\n");
        struct sockaddr_in6 boundAddr;
        #ifdef _WIN32
        int addrLen = sizeof(boundAddr);
        #else
        socklen_t addrLen = sizeof(boundAddr);
        #endif

        if (getsockname(listenSock, (struct sockaddr*)&boundAddr, &addrLen) == 0) {
            server_port = boundAddr.sin6_port;
            print("server started on port %lld\n", (int64_t)ntohs(boundAddr.sin6_port));
        }
        else
        {
            #ifdef _WIN32
            print("get name failed %lld\n", WSAGetLastError());
            #else
            print("get name failed %s\n", strerror(errno));
            #endif
        }
    }
    else
    {
        #ifdef _WIN32
        print("Error happen %lld\n", WSAGetLastError());
        #else
        print("Error happen %lld\n", strerror(errno));
        #endif
    }
    
    listen(listenSock, SOMAXCONN);
    
    while (1)
    {
        #ifdef _WIN32
        SOCKADDR_STORAGE storage;
        int storage_len = sizeof(storage);
        socket_t client = accept(listenSock, (SOCKADDR *)&storage, &storage_len);
        if (client != INVALID_SOCKET)
        {
            HandleNewConnection(client, storage, storage_len);
        }
        #else
        struct sockaddr_storage storage;
        socklen_t storage_len = sizeof(storage);
        int client = accept(listenSock, (struct sockaddr *)&storage, &storage_len);
        if (client != INVALID_SOCKET)
        {
            HandleNewConnection(client, storage, storage_len);
        }
        else if (errno == EINTR) 
        {
            continue;
        }
        else 
        {
            log("Accept error: %s\n", strerror(errno));
        }
        #endif
    }
    
    return 0;
}


void GetsetInsertTagged(struct hashtable * _Atomic *table, void *key, void *new_node)
{    
    // select all data on this query from hashtable
    while (1)
    {
        int64_t value = GetHashtable(table, key);
        int64_t tag = value & GETSET_WAIT_LIST_VALUE_PROCESSING_TAG;
        *(void **)new_node = (void *)(value & (~GETSET_WAIT_LIST_VALUE_PROCESSING_TAG)); // set next pointer, and remove tag from it
        int64_t tagged_new_node = tag + (int64_t)new_node;
        if (SetHashtable(&get_wait_list, key, tagged_new_node, value) == value)
        {
            // node updated, tag copied.
            return;
        }
    }
}


static void UpdateWaitingQueryList(int64_t object_id, int64_t offset, int64_t size, BYTE *data, struct get_wait_list_value *q)
{
    // call callbacks on each wait item
    while (q != NULL)
    {
        q->callback(object_id, offset, size, data, q->params);
        void *tmp = q;
        q = q->next;
        myFree(tmp);
    }
}

void UpdateWaitingQuery(int64_t object_id, int64_t offset, int64_t size, BYTE *data)
{    
    struct get_wait_list_key key = { object_id, offset, size };
    
    int64_t value = TakeTaggedHashtable(&get_wait_list, &key, GETSET_WAIT_LIST_VALUE_PROCESSING_TAG, GETSET_WAIT_LIST_PARALLEL_PROCESSING);
    struct get_wait_list_value *q = (void *)(value & (~GETSET_WAIT_LIST_VALUE_PROCESSING_TAG));
    // ! is q is null, no tag will be created

    // now, q is top pointer, and hashtable is tagged
    if (q != NULL)
    {
        UpdateWaitingQueryList(object_id, offset, size, data, q);
        // and now, release tag
        AddHashtable(&get_wait_list, &key, -1);
    }
    
    return;
}

static void UpdateWaitingPushList(int64_t object_id, int64_t offset, int64_t size, int64_t hash, struct set_wait_list_value *q)
{
    // call callbacks on each wait item
    while (q != NULL)
    {
        q->callback(object_id, offset, size, hash, q->params);
        void *tmp = q;
        q = q->next;
        myFree(tmp);
    }
    
    return;
}

void UpdateWaitingPush(int64_t object_id, int64_t offset, int64_t size, int64_t hash)
{
    struct set_wait_list_key key = { object_id, offset, size, hash };

    int64_t value = TakeTaggedHashtable(&set_wait_list, &key, GETSET_WAIT_LIST_VALUE_PROCESSING_TAG, GETSET_WAIT_LIST_PARALLEL_PROCESSING);
    struct set_wait_list_value *q = (void *)(value & (~GETSET_WAIT_LIST_VALUE_PROCESSING_TAG));
    // ! is q is null, no tag will be created
    
    // now, q is top pointer, and hashtable is tagged
    if (q != NULL)
    {
        UpdateWaitingPushList(object_id, offset, size, hash, q);
        // and now, release tag
        AddHashtable(&get_wait_list, &key, -1);
    }
    return;
}

#define API_REQUEST_CONNECTION 0x00
#define API_REQUEST_MEM_PAGE 0x02
#define API_QUERY_OBJECT 0x04
#define API_QUERY_PIPE 0x24
#define API_PUSH_OBJECT 0x06
#define API_PUSH_PIPE 0x26
#define API_REQUEST_PATH 0x08
#define API_CALL_WORKER 0x10
#define API_GET_HIVE_STATE 0x12
#define API_REQUEST_ID 0xFE
#define API_REQUEST_PATH_TO_ID 0xFC

#define API_ANSWER_REQUEST_CONNECTION (API_REQUEST_CONNECTION | 0x1)
#define API_ANSWER_REQUEST_MEM_PAGE (API_REQUEST_MEM_PAGE | 0x1)
#define API_ANSWER_QUERY_OBJECT (API_QUERY_OBJECT | 0x1)
#define API_ANSWER_QUERY_PIPE (API_QUERY_PIPE | 0x1)
#define API_ANSWER_PUSH_OBJECT (API_PUSH_OBJECT | 0x1)
#define API_ANSWER_PUSH_PIPE (API_PUSH_PIPE | 0x1)
#define API_ANSWER_REQUEST_PATH (API_REQUEST_PATH | 0x1)
#define API_ANSWER_REQUEST_ID (API_REQUEST_ID | 0x1)
#define API_ANSWER_REQUEST_PATH_TO_ID (API_REQUEST_PATH_TO_ID | 0x1)

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
        
        API_QUERY_OBJECT | API_QUERY_PIPE
            8 byte: object_id
            8 byte: offset
            8 byte: size
            27 byte: query ID [if API_QUERY_PIPE]

        API_PUSH_OBJECT | API_PUSH_PIPE
            8 byte: object_id
            8 byte: offset
            8 byte: size
            27 byte: push ID [if API_PUSH_PIPE]
            ----
            raw bytes

        API_REQUEST_PATH
            8 byte: object_id
            27 byte: broadcast ID

        API_CALL_WORKER
            8 byte: worker_id
            8 byte: global_id_parameter
            ----
            raw bytes

        API_GET_HIVE_STATE:
            8 byte: wait_list_len
            8 byte: queue_len
            8 byte: idle_time [in microseconds]

        API_REQUEST_ID:
            8 byte: proposed id
            27 byte: broadcast ID

        API_REQUEST_PATH_TO_ID:
            8 byte: global_id
            27 byte: broadcast ID

    api ANSWERS:

        API_ANSWER_REQUEST_CONNECTION:
            8 byte: reply_id
            
        API_ANSWER_REQUEST_MEM_PAGE
            27 byte: broadcast ID

        API_ANSWER_QUERY_OBJECT | API_ANSWER_QUERY_PIPE
            8 byte: object_id
            8 byte: offset
            8 byte: size
            27 byte: query ID [if API_ANSWER_QUERY_PIPE]
            ----
            raw bytes

        API_ANSWER_PUSH_OBJECT | API_ANSWER_PUSH_PIPE
            8 byte: object_id
            8 byte: offset
            8 byte: size
            8 byte: hash

        API_ANSWER_REQUEST_PATH
            8 byte: object_id
            8 byte: result_distance

        API_ANSWER_REQUEST_ID:
            27 byte: broadcast ID
            
        API_ANSWER_REQUEST_PATH_TO_ID:
            8 byte: global_id
            8 byte: distance
*/


/*---------------------------------------------- receive api logic ---------------------------------------------*/
static int64_t HandleApiCall(struct hive_connection *con)
{
    struct connection_context *ctx = con->ctx;
    log("Get api call %lld of length: %lld [con=%p]\n", ctx->res_api_call, ctx->res_buffer_len, con);
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
            int64_t local_id = *(int64_t *)ctx->res_buffer;
            log("API_ANSWER_REQUEST_CONNECTION updating using local_id=%lld\n", local_id);
            
            // update that id
            lock_write(&connections_lock);
            for (int64_t i = 0; i < connections_len; ++i)
            {
                if (connections[i]->local_id == local_id)
                {
                    con->outgoing = connections[i]->outgoing;
                    
                    int64_t index;
                    struct hive_connection *old_con = GetConnectionById(local_id, &index);
                    log("Updated using [con=%p] [and free it]\n", old_con);
                    myFree(old_con);
                    connections[index] = connections[--connections_len];
                    unlock_write(&connections_lock);
                    return 1;
                }
            }
            print("Error: API_ANSWER_REQUEST_CONNECTION becouse there is no connection with local_id=%lld\n", local_id);
            unlock_write(&connections_lock);
            return 0;
        }
        case API_REQUEST_ID:
        {
            int64_t want_id = *(int64_t *)ctx->res_buffer;
            BYTE *broadcast_id = ctx->res_buffer + 8;
            log("API_REQUEST_ID page=%lld [prefix=%llx]\n", want_id, *(int64_t *)broadcast_id);
            // check - is id used [or we want to use this id]
            if (thisServerId == want_id)
            {
                log("Refuse id\n");
                return 1;
            }
            
            // if we know anybody who also want's to use this id, say no
            int64_t timeout = i64GetHashtable(&get_id_ids, want_id);
            if (timeout != 0 && GetTicks() < timeout)
            {
                // also refuse
                log("Refuse becouse it is repeated request\n");
                return 1;
            }
            
            // check - if we already answering this broadcast
            struct get_id_broadcasts_value *broadcast = (void *)GetHashtable(&get_id_broadcasts, broadcast_id);
            
            if (broadcast == NULL) // new, create it in table
            {
                broadcast = myMalloc(sizeof(*broadcast));
                broadcast->id = want_id;
                broadcast->local_redirect_id = con->local_id;
                broadcast->answered = 0;
                broadcast->requested = 0;
                if (SetHashtable(&get_id_broadcasts, broadcast_id, (int64_t)broadcast, 0) == 0)
                {
                    // all is good, broadcast created, redirect it and end processing
                    RedirectBroadcastIDQuery(want_id, broadcast_id, con->local_id, &broadcast->requested);
                    return 1;
                }
                
                myFree(broadcast);
                
                // somebody already created same broadcast, so, use it
                broadcast = (void *)GetHashtable(&get_id_broadcasts, broadcast_id);
                
                assert(broadcast != NULL);
            }
            
            // to disable circular requests, if this request is not from itiniator of this broadcast on this
            // hive, we answer yes.
            if (broadcast->local_redirect_id != con->local_id)
            {
                log("Confirmed becouse this is not initiator.\n");
                SendIDConfirm(con, broadcast_id);
                return 1;
            }

            int64_t answered = broadcast->answered;
            int64_t requested = broadcast->requested;

            // is broadcast accepted by all neibours?
            if (answered == requested)
            {
                SendIDConfirm(con, broadcast_id);
                return 1;
            }

            // else - waiting for more acceptions
            return 1;
        }
        case API_ANSWER_REQUEST_ID:
        {
            BYTE *broadcast_id = ctx->res_buffer;
            // get broadcast
            log("Get broadcast answer [prefix=%llx]\n", *(int64_t *)broadcast_id);
            
            
            struct get_id_broadcasts_value *broadcast = (void *)GetHashtable(&get_id_broadcasts, broadcast_id);
            
            if (broadcast == NULL) return 1; // ignore incorrect answers == refuse them
            
            int64_t new_count = ++broadcast->answered;
            
            // if this was last answer from neibours,
            // answer to initiator of this broadcast.
            if (new_count == broadcast->requested)
            {
                // if initiator is this hive, update server's id
                if (broadcast->local_redirect_id == -1)
                {
                    log("local id broadcast confirmed\n");
                    ConfirmID(broadcast->id);
                }
                else
                {
                    log("remote id broadcast confirmed\n");
                    
                    lock_read(&connections_lock);
                    struct hive_connection *ansCon = GetConnectionById(broadcast->local_redirect_id, NULL);
                    unlock_read(&connections_lock);
                    if (ansCon)
                    {
                        SendIDConfirm(ansCon, broadcast_id);
                    }
                }
            }
            
            return 1;
        }
        case API_REQUEST_MEM_PAGE:
        {
            glbStatRemoteInputRequests++;
            int64_t page_id;
            memcpy(&page_id, ctx->res_buffer, 5);
            BYTE *broadcast_id = ctx->res_buffer + 5;
            
            log("API_REQUEST_MEM_PAGE page=%lld [prefix=%llx]\n", page_id, *(int64_t *)broadcast_id);
            // check - is page used?
            lock_read(&pages_lock);
            for (int64_t i = 0; i < pages_len; ++i)
            {
                if (pages[i].id == page_id)
                {
                    // refuse
                    log("Refuse allocation\n");
                    unlock_read(&pages_lock);
                    return 1;
                }
            }
            unlock_read(&pages_lock);

            
            // if we know anybody who also want's to use this id, say no
            int64_t timeout = i64GetHashtable(&get_page_ids, page_id);
            if (timeout != 0 && GetTicks() < timeout)
            {
                // also refuse
                log("Refuse becouse it is repeated request\n");
                return 1;
            }
            
            // check - if we already answering this broadcast
            struct get_page_broadcasts_value *broadcast = (void *)GetHashtable(&get_page_broadcasts, broadcast_id);
            
            if (broadcast == NULL) // new, create it in table
            {
                broadcast = myMalloc(sizeof(*broadcast));
                broadcast->page_id = page_id;
                broadcast->local_redirect_id = con->local_id;
                broadcast->answered = 0;
                broadcast->requested = 0;
                if (SetHashtable(&get_id_broadcasts, broadcast_id, (int64_t)broadcast, 0) == 0)
                {
                    // all is good, broadcast created, redirect it and end processing
                    RedirectBroadcastQuery(page_id, broadcast_id, con->local_id, &broadcast->requested);
                    return 1;
                }
                
                myFree(broadcast);
                
                // somebody already created same broadcast, so, use it
                broadcast = (void *)GetHashtable(&get_id_broadcasts, broadcast_id);
                
                assert(broadcast != NULL);
            }
            
            // to disable circular requests, if this request is not from itiniator of this broadcast on this
            // hive, we answer yes.
            if (broadcast->local_redirect_id != con->local_id)
            {
                log("Confirmed becouse this is not initiator.\n");
                SendPageAllocationConfirm(con, broadcast_id);
                return 1;
            }
                        
            int64_t answered = broadcast->answered;
            int64_t requested = broadcast->requested;

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
            log("Get page broadcast answer [prefix=%llx]\n", *(int64_t *)broadcast_id);
            
            
            struct get_page_broadcasts_value *broadcast = (void *)GetHashtable(&get_page_broadcasts, broadcast_id);
            
            if (broadcast == NULL) return 1; // ignore incorrect answers == refuse them
            
            int64_t new_count = ++broadcast->answered;
            
            // if this was last answer from neibours,
            // answer to initiator of this broadcast.
            if (new_count == broadcast->requested)
            {
                // if initiator is this hive, update server's id
                if (broadcast->local_redirect_id == -1)
                {
                    log("local id broadcast confirmed\n");
                    ConfirmPage(broadcast->page_id);
                }
                else
                {
                    log("remote id broadcast confirmed\n");
                    
                    lock_read(&connections_lock);
                    struct hive_connection *ansCon = GetConnectionById(broadcast->local_redirect_id, NULL);
                    unlock_read(&connections_lock);
                    if (ansCon)
                    {
                        SendPageAllocationConfirm(ansCon, broadcast_id);
                    }
                }
            }
            return 1;
        }
        case API_QUERY_OBJECT:
        {
            glbStatRemoteInputRequests++;
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t offset = *(int64_t *)(ctx->res_buffer+8);
            int64_t size = *(int64_t *)(ctx->res_buffer+16);
            
            // is this object local?
            struct object *obj = (void *)i64GetHashtable(&local_objects, object_id);
            if (obj != NULL)
            {
                AnswerQueryObject(con, (BYTE *)obj + offset, object_id, offset, size);
                return 1;
            }
            
            struct get_wait_list_value *new_value = myMalloc(sizeof(*new_value));
            new_value->params = (void *)con->local_id;
            new_value->callback =  callbackQueryAnswerLocalId;
            
            struct get_wait_list_key key = { object_id, offset, size };
            GetsetInsertTagged(&get_wait_list, &key, new_value);
            
            // repeat request, (to 100% find destination hive)
            RequestObjectGet(object_id, offset, size, NULL);
            
            return 1;
        }
        case API_ANSWER_QUERY_OBJECT:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t query_offset = *(int64_t *)(ctx->res_buffer+8);
            int64_t query_size = *(int64_t *)(ctx->res_buffer+16);
            BYTE *data = ctx->res_buffer+24;
            
            UpdateWaitingQuery(object_id, query_offset, query_size, data);
            
            return 1;
        }
        case API_PUSH_OBJECT:
        {
            glbStatRemoteInputRequests++;
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t offset = *(int64_t *)(ctx->res_buffer+8);
            int64_t size = *(int64_t *)(ctx->res_buffer+16);
            BYTE *data = ctx->res_buffer + 24;
            
            int64_t hash = GetByteStringHash(data, size);
            
            // is this object local?
            struct object *obj = (void *)i64GetHashtable(&local_objects, object_id);
            if (obj != NULL)
            {
                log("remote push - local\n");
                universalUpdateLocalPush(obj, offset, size, data);
                AnswerPushObject(con, object_id, offset, size, hash);
                UpdateWaitingPush(object_id, offset, size, hash);
                return 1;
            }
            log("remote push - redirect\n");
            
            struct set_wait_list_value *new_value = myMalloc(sizeof(*new_value) + 16);
            new_value->params = (void *)con->local_id;
            new_value->callback = callbackPushAnswerLocalId;
            
            struct set_wait_list_key key = { object_id, offset, size, hash };
            GetsetInsertTagged(&get_wait_list, &key, new_value);
            
            // repeat request, (to 100% find destination hive)
            RequestObjectGet(object_id, offset, size, NULL);
            
            // TODO: store data in some place, and repeat call manually after some time
            //       [now this makes server who want this push...]
            RequestObjectSet(object_id, offset, size, data, NULL);
            return 1;
        }
        case API_ANSWER_PUSH_OBJECT:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t offset = *(int64_t *)(ctx->res_buffer+8);
            int64_t size = *(int64_t *)(ctx->res_buffer+16);
            int64_t hash = *(int64_t *)(ctx->res_buffer+24);
            
            /* for each program in waiting list - continue if this is it's request */
            /* for each waiting local_id - answer */
            UpdateWaitingPush(object_id, offset, size, hash);
            
            return 1;
        }
        case API_REQUEST_PATH:
        {
            glbStatRemoteInputRequests++;
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            BYTE *broadcast_id = ctx->res_buffer + 8;
            
            
            // is this object local?
            struct object *obj = (void *)i64GetHashtable(&local_objects, object_id);
            if (obj != NULL)
            {
                AnswerRequestObjectPath(object_id, 1);
                return 1;
            }
            
            // else - if broadcast is new, redirect it, else - ignore
            if (GetHashtable(&get_path_broadcasts, broadcast_id) == 0)
            {
                RequestObjectPathBroadcast(object_id, con->local_id);
                
                // here it is not interesting for us if there was more than one hashtable update.
                if (SetHashtable(&get_path_broadcasts, broadcast_id, 1, 0) != 0)
                {
                    // somebody was already created this path, but this don't change something.
                }
            }
            return 1;
        }
        case API_ANSWER_REQUEST_PATH:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t distance = *(int64_t *)(ctx->res_buffer + 8);
            
            
            // get object - if it is local - don't update anything, and don't send answers
            struct object *obj = (void *)i64GetHashtable(&local_objects, object_id);
            if (obj != NULL)
            {
                return 1;
            }
            
            
            /* update known objects base */
            while (1)
            {
                struct object_paths_value *path = (void *)i64GetHashtable(&object_paths, object_id);
                if (path != NULL && path->distance > distance)
                {
                    log("UPDATE PATH 1 TO %lld [distance=%lld, local_id=%lld]\n", object_id, distance, con->local_id);
                    path->local_id = con->local_id;
                    path->distance = distance;
                    break;
                }
                else if (path == NULL)
                {
                    log("UPDATE PATH 2 TO %lld [distance=%lld, local_id=%lld]\n", object_id, distance, con->local_id);
                    path = myMalloc(sizeof(*path));
                    path->local_id = con->local_id;
                    path->distance = distance + 1;
                    if (i64SetHashtable(&object_paths, object_id, (int64_t)path, 0) != 0)
                    {
                        // path to this object was changed
                        myFree(path);
                        continue;
                    }
                }
                else
                {
                    // this path is bad, ignore it
                    return 1;
                }
            }
            
            AnswerRequestObjectPath(object_id, distance + 1);
            return 1;
        }
        case API_REQUEST_PATH_TO_ID:
        {
            glbStatRemoteInputRequests++;
            int64_t global_id = *(int64_t *)(ctx->res_buffer);
            BYTE *broadcast_id = ctx->res_buffer + 8;
            
            // if this is our server id - answer
            if (global_id == thisServerId)
            {
                AnswerRequestPathToID(global_id, 1);
                return 1;
            }
            
            // redirect broadcast if it is new.
            if (GetHashtable(&get_path_to_id_broadcasts, broadcast_id) == 0)
            {
                RequestObjectPathBroadcast(global_id, con->local_id);
                
                // here it is not interesting for us if there was more than one hashtable update.
                if (SetHashtable(&get_path_to_id_broadcasts, broadcast_id, 1, 0) != 0)
                {
                    // somebody was already created this path, but this don't change something.
                }
            }
            return 1;
        }
        case API_ANSWER_REQUEST_PATH_TO_ID:
        {
            int64_t global_id = *(int64_t *)(ctx->res_buffer);
            int64_t distance = *(int64_t *)(ctx->res_buffer + 8);
            
            // get update of id - if it is ours - don't update anything, and don't send answers
            if (global_id == thisServerId)
            {
                return 1;
            }
            
            while (1)
            {
                struct object_paths_value *path = (void *)i64GetHashtable(&global_id_paths, global_id);
                if (path != NULL && path->distance > distance)
                {
                    log("UPDATE ID PATH 1 TO %lld [distance=%lld, local_id=%lld]\n", global_id, distance, con->local_id);
                    path->local_id = con->local_id;
                    path->distance = distance;
                    break;
                }
                else if (path == NULL)
                {
                    log("UPDATE ID PATH 2 TO %lld [distance=%lld, local_id=%lld]\n", global_id, distance, con->local_id);
                    path = myMalloc(sizeof(*path));
                    path->local_id = con->local_id;
                    path->distance = distance + 1;
                    if (i64SetHashtable(&global_id_paths, global_id, (int64_t)path, 0) != 0)
                    {
                        // path to this object was changed
                        myFree(path);
                        continue;
                    }
                }
                else
                {
                    // this path is bad, ignore it
                    return 1;
                }
            }
            
            AnswerRequestPathToID(global_id, distance + 1);
            return 1;
        }
        case API_CALL_WORKER:
        {
            glbStatRemoteInputRequests++;
            int64_t worker_id = *(int64_t *)(ctx->res_buffer+0);
            int64_t global_id = *(int64_t *)(ctx->res_buffer+8);
            BYTE *data = ctx->res_buffer + 16;
            log("Get remote start worker %lld from local_id=%lld on %lld\n", worker_id, con->local_id, global_id);
            /* load data */
            for (int i = 0; i < Workers[worker_id].inputSize; ++i)
            {
                log("%02x ", data[i]);
            }
            log("\n");
            StartNewWorker(worker_id, global_id, data);
            return 1;
        }
        case API_GET_HIVE_STATE:
        {
            int64_t it_wait_list_len = *(int64_t *)(ctx->res_buffer);
            int64_t it_queue_len = *(int64_t *)(ctx->res_buffer + 8);
            int64_t it_idle_time = *(int64_t *)(ctx->res_buffer + 16);
            log("GET HIVE STATE: %lld %lld %lld [con=%p]\n", it_wait_list_len, it_queue_len, it_idle_time, con);
            con->wait_list_len = it_wait_list_len;
            con->queue_len = it_queue_len;
            con->idle_time = it_idle_time;
            return 1;
        }
    }
    return 0;
}

#ifdef _WIN32
static DWORD Worker(void *param)
#else
static void *Worker(void *param)
#endif
{
    (void)param;
    
    DWORD bytesReceived, bytesProcessed;
    struct connection_context *ctx;

    #ifdef _WIN32
    ULONG_PTR completionKey;
    LPOVERLAPPED lpOverlapped;
    #else
    struct epoll_event events[64];
    int event_idx = 0, num_events = 0;
    #endif

    while (1) 
    {
        #ifdef _WIN32
        if (!GetQueuedCompletionStatus(hIOCP, &bytesReceived, &completionKey, &lpOverlapped, INFINITE))
        {
            break;
        }
        ctx = (struct connection_context *)lpOverlapped;
        #else
        if (event_idx >= num_events) {
            num_events = epoll_wait(hEpoll, events, 64, -1);
            if (num_events <= 0) continue;
            event_idx = 0;
        }
        ctx = (struct connection_context *)events[event_idx++].data.ptr;
        ssize_t res = recv(ctx->socket, ctx->buffer, sizeof(ctx->buffer), 0);
        if (res < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            continue;
        }
        bytesReceived = (res <= 0) ? 0 : (DWORD)res;
        #endif
    
        if (bytesReceived == 0)
        {
            print("[NETWORK]: Peer disconnected\n");
            lock_write(&connections_lock);
            int64_t index;
            GetConnectionById(ctx->connection->local_id, &index);
            connections[index] = connections[--connections_len];
            unlock_read(&connections_lock);

            if (ctx->connection->outgoing.sock != INVALID_SOCKET)
            {
                closesocket(ctx->connection->outgoing.sock);
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
                        print("connection was closed. [local_id = %lld]\n", ctx->connection->local_id);
                        // delete this connection.
                        deleteConnection = 1;
                        closesocket(ctx->socket);
                        if (ctx->connection->outgoing.sock != INVALID_SOCKET)
                        {
                            closesocket(ctx->connection->outgoing.sock);
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
            #ifdef _WIN32
            DWORD flags = 0;
            WSARecv(ctx->socket, &ctx->wsaBuf, 1, NULL, &flags, &ctx->overlapped, NULL);
            #endif
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
    lock_write(&con->lock);
    emitData(con->outgoing, (char *)message, sizeof(message), 0);
    unlock_write(&con->lock);
}

void SendIDConfirm(struct hive_connection *con, BYTE *broadcast_id)
{
    log("Send confirmation of page allocation to local_id=%lld\n", con->local_id);
    BYTE message[8+27] = {API_ANSWER_REQUEST_ID, BROADCAST_ID_LENGTH, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8, broadcast_id, BROADCAST_ID_LENGTH);
    lock_write(&con->lock);
    emitData(con->outgoing, (char *)message, sizeof(message), 0);
    unlock_write(&con->lock);
}


void RedirectBroadcastQuery(int64_t page_id, BYTE *broadcast_id, int64_t except_this_local_id, _Atomic int32_t *send_counter)
{
    lock_read(&connections_lock);
    BYTE message[8+5+27] = {API_REQUEST_MEM_PAGE, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8,  &page_id, 5);
    memcpy(message + 8+5, broadcast_id, BROADCAST_ID_LENGTH);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id != except_this_local_id)
        {
            log("Redirecting page query to local_id=%lld\n", connections[i]->local_id);
            lock_write(&connections[i]->lock);
            emitData(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            unlock_write(&connections[i]->lock);
            (*send_counter)++;
        }
    }
    unlock_read(&connections_lock);
}


void RedirectBroadcastIDQuery(int64_t want_id, BYTE *broadcast_id, int64_t except_this_local_id, _Atomic int32_t *send_counter)
{
    lock_read(&connections_lock);
    BYTE message[8+8+27] = {API_REQUEST_ID, 8+27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8,  &want_id, 8);
    memcpy(message + 8+8, broadcast_id, BROADCAST_ID_LENGTH);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id != except_this_local_id)
        {
            log("Redirecting id query to local_id=%lld\n", connections[i]->local_id);
            lock_write(&connections[i]->lock);
            emitData(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            unlock_write(&connections[i]->lock);
            (*send_counter)++;
        }
    }
    unlock_read(&connections_lock);
}


void ConfirmConnection(struct hive_connection *ctx, int64_t reply_id, int64_t port)
{
    socket_t s = INVALID_SOCKET;

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
        #if NODELAY_MODE == 1
            int32_t yes = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(yes));
        #endif
        
        log("Connection is successful! [1]\n");
        lock_read(&connections_lock);
        ctx->outgoing = (struct bufferized_socket){s, 0, {}};
        unlock_read(&connections_lock);

        BYTE header[8] = {0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        emitData(ctx->outgoing, (char *) header, sizeof(header), 0);
        emitData(ctx->outgoing, (char *)&reply_id, 8, 0);
        timedFlush(ctx->outgoing);
    }
}

int64_t InitiateConnection(const char *host, const char *port)
{
    if (server_port == -1)
    {
        log("Error - server_port == -1 [server isn't started]\n");
        return -1;
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
    
    socket_t s = INVALID_SOCKET;
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
        #if NODELAY_MODE == 1
            int32_t yes = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(yes));
        #endif
        
        log("Connection is successful! [2]\n");
        
        lock_write(&connections_lock);
        
        int64_t local_id = next_local_id++;
        int64_t conn = connections_len++;

        
        connections[conn] = myMalloc(sizeof(**connections));
        struct hive_connection *con = connections[conn];
        
        connections[conn]->lock = (lock_t)INIT_LOCK;
        connections[conn]->ctx = NULL;
        connections[conn]->outgoing = (struct bufferized_socket){s, 0, {}};
        connections[conn]->local_id = local_id;
        
        connections[conn]->wait_list_len = INT_INFINITY;
        connections[conn]->queue_len = INT_INFINITY;
        connections[conn]->idle_time = 0;
        
        unlock_write(&connections_lock);
        
        BYTE header[8] = {API_REQUEST_CONNECTION, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        emitData(con->outgoing, (char *) header, sizeof(header), 0);
        emitData(con->outgoing, (char *)&local_id, 8, 0);
        emitData(con->outgoing, (char *)&server_port, 8, 0);
        timedFlush(con->outgoing);
        return local_id;
    } 
    else 
    {
        print("Error: Could not connect to %s [error=%lld]\n", host, (int64_t)GetLastError());
        return -1;
    }
}


void RequestMemoryPage(int64_t page_id)
{
    log("Trying to get memory page %lld\n", page_id);
    glbStatRemoteOutputRequests++;

    // create random seed
    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    SECURE_RANDOM(broadcast_id, BROADCAST_ID_LENGTH);
    
    // request page from all neibours
    struct memory_page_request *broadcast = myMalloc(sizeof(*broadcast));
    broadcast->page_id = page_id;
    broadcast->local_redirect_id = -1; // this hive
    broadcast->answered = 0;
    broadcast->requested = 0;
    
    while (SetHashtable(&get_page_broadcasts, broadcast_id, (int64_t)broadcast, 0) != 0)
    {
        // this id is already used - create another one
        SECURE_RANDOM(broadcast_id, BROADCAST_ID_LENGTH);
    }
    
    log("Created broadcast with prefix=%llx\n", *(int64_t *)broadcast_id);

    // redirect queries
    RedirectBroadcastQuery(page_id, broadcast_id, -1, &broadcast->requested);
    if (broadcast->requested == 0)
    {
        // confirm page alloaction
        ConfirmPage(page_id);
    }
}

void RequestServerId(int64_t new_id)
{
    log("Trying to get server id %lld\n", new_id);

    // create random seed
    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    SECURE_RANDOM(broadcast_id, BROADCAST_ID_LENGTH);
    
    struct get_id_broadcasts_value *broadcast = myMalloc(sizeof(*broadcast));
    broadcast->id = new_id;
    broadcast->local_redirect_id = -1; // this hive
    broadcast->answered = 0;
    broadcast->requested = 0;
    
    
    while (SetHashtable(&get_id_broadcasts, broadcast_id, (int64_t)broadcast, 0) != 0)
    {
        SECURE_RANDOM(broadcast_id, BROADCAST_ID_LENGTH);
    }

    log("Created broadcast with prefix=%llx\n", *(int64_t *)broadcast_id);

    // redirect queries
    RedirectBroadcastIDQuery(new_id, broadcast_id, -1, &broadcast->requested);
    if (broadcast->requested == 0)
    {
        ConfirmID(new_id);
    }
}


void AnswerPushObject(struct hive_connection *con, int64_t object_id, int64_t offset, int64_t size, int64_t hash)
{    
    log("Answer Push Object to localid=%lld [%lld+%lld:%lld]\n", con->local_id, object_id, offset, size);
    BYTE header[8] = {API_ANSWER_PUSH_OBJECT, 32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    lock_write(&con->lock);
    emitData(con->outgoing, (char *) header, sizeof(header), 0);
    emitData(con->outgoing, (char *)&object_id, 8, 0);
    emitData(con->outgoing, (char *)&offset, 8, 0);
    emitData(con->outgoing, (char *)&size,   8, 0);
    emitData(con->outgoing, (char *)&hash,   8, 0);
    unlock_write(&con->lock);
}

void AnswerQueryObject(struct hive_connection *con, void *shifted_buffer, int64_t object_id, int64_t offset, int64_t size)
{    
    log("Answer Query Object to localid=%lld [%lld+%lld:%lld]\n", con->local_id, object_id, offset, size);
    BYTE header[8] = {API_ANSWER_QUERY_OBJECT, 24+size, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    lock_write(&con->lock);
    emitData(con->outgoing, (char *) header, sizeof(header), 0);
    emitData(con->outgoing, (char *)&object_id, 8, 0);
    emitData(con->outgoing, (char *)&offset, 8, 0);
    emitData(con->outgoing, (char *)&size,   8, 0);
    // send data
    emitData(con->outgoing, (char *)shifted_buffer, size, 0);
    unlock_write(&con->lock);
}
                

void AnswerRequestObjectPath(int64_t object, int64_t distance)
{
    BYTE message[8+16] = {API_ANSWER_REQUEST_PATH, 16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8, &object, 8);
    memcpy(message + 16, &distance, 8);
    lock_read(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL)
        {
            log("send broadcast answer path to local_id=%lld\n", connections[i]->local_id);
            lock_write(&connections[i]->lock);
            emitData(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            unlock_write(&connections[i]->lock);
        }
    }
    unlock_read(&connections_lock);
}


void RequestObjectPathBroadcast(int64_t object, int64_t except_this_local_id)
{
    // update known_object structure
    
    struct object_paths_value *obj = (void *)i64GetHashtable(&object_paths, object);
    if (obj != NULL)
    {
        obj->distance = INFINITY_DISTANCE;
    }

    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    SECURE_RANDOM(broadcast_id, BROADCAST_ID_LENGTH);

    BYTE message[8+8+27] = {API_REQUEST_PATH, 8+27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8,  &object, 8);
    memcpy(message + 8+8, broadcast_id, BROADCAST_ID_LENGTH);
    lock_read(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id != except_this_local_id)
        {
            log("send broadcast path request to local_id=%lld\n", connections[i]->local_id);
            lock_write(&connections[i]->lock);
            emitData(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            unlock_write(&connections[i]->lock);
        }
    }
    unlock_read(&connections_lock);
}



void AnswerRequestPathToID(int64_t global_id, int64_t distance)
{
    BYTE message[8+16] = {API_ANSWER_REQUEST_PATH, 16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8, &global_id, 8);
    memcpy(message + 16, &distance, 8);
    lock_read(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL)
        {
            log("send broadcast answer id path to local_id=%lld\n", connections[i]->local_id);
            lock_write(&connections[i]->lock);
            emitData(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            unlock_write(&connections[i]->lock);
        }
    }
    unlock_read(&connections_lock);
}


void RequestPathToIDBroadcast(int64_t global_id, int64_t except_this_local_id)
{
    // update known_object structure
    
    struct global_id_paths_value *obj = (void *)i64GetHashtable(&global_id_paths, global_id);
    if (obj != NULL)
    {
        obj->distance = INFINITY_DISTANCE;
    }

    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    SECURE_RANDOM(broadcast_id, BROADCAST_ID_LENGTH);

    BYTE message[8+8+27] = {API_REQUEST_PATH, 8+27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8,  &global_id, 8);
    memcpy(message + 8+8, broadcast_id, BROADCAST_ID_LENGTH);
    lock_read(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id != except_this_local_id)
        {
            log("send broadcast path request to local_id=%lld\n", connections[i]->local_id);
            lock_write(&connections[i]->lock);
            emitData(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            unlock_write(&connections[i]->lock);
        }
    }
    unlock_read(&connections_lock);
}


void RequestObjectGet(int64_t object_id, int64_t offset, int64_t size, struct waiting_worker *worker)
{
    // find object in object table
    struct object_paths_value *obj = (void *)i64GetHashtable(&object_paths, object_id);
    if (obj == NULL || obj->local_id == -1)
    {
        // we need to find path
        log("Sending broadcast to find object=%lld\n", object_id);
        glbStatRemotePathMisses++;
        RequestObjectPathBroadcast(object_id, -1);
        // now, we can't resolve request
        return;
    }

    // add worker as waiting for request
    if (worker)
    {
        log("Wait hashtable list insert %p worker [read]\n", worker);
        struct linked_node *new_node = myMalloc(sizeof(*new_node));
        new_node->local_id = (int64_t)worker;
        
        struct get_wait_list_value *new_value = myMalloc(sizeof(*new_value));
        new_value->params = (void *)worker;
        new_value->callback = callbackContinueWorkerFromWaitingQuery;
        
        struct get_wait_list_key key = { object_id, offset, size };
        
        GetsetInsertTagged(&get_wait_list, &key, new_value);
    }

    glbStatRemoteOutputRequests++;
    
    // now, we know which hive handles object - request it from him
    struct hive_connection *connection = GetConnectionById(obj->local_id, NULL);

    BYTE header[8] = {API_QUERY_OBJECT, 24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    lock_write(&connection->lock);
    emitData(connection->outgoing, (char *) header, sizeof(header), 0);
    emitData(connection->outgoing, (char *)&object_id, 8, 0);
    emitData(connection->outgoing, (char *)&offset, 8, 0);
    emitData(connection->outgoing, (char *)&size,   8, 0);
    unlock_write(&connection->lock);
}

void RequestObjectSet(int64_t object_id, int64_t offset, int64_t size, void *data, struct waiting_worker *worker)
{
    log("request set object=%lld\n", object_id);
    
    int64_t hash = GetByteStringHash(data, size);
    
    // if this is local object - simply set it and answer [to who?]
    struct object *loc = (void *)i64GetHashtable(&local_objects, object_id);
    if (loc != NULL)
    {
        log("local object - answer\n");
        universalUpdateLocalPush(loc, offset, size, data);
        /* update all local waiting processes */
        UpdateWaitingPush(object_id, offset, size, hash);
        // it was found
        return;
    }

    // add worker as waiting for push on this path
    if (worker)
    {
        log("Wait hashtable list insert %p worker [write]\n", worker);
        
        struct linked_node *new_node = myMalloc(sizeof(*new_node));
        new_node->local_id = (int64_t)worker;
        
        struct set_wait_list_value *new_value = myMalloc(sizeof(*new_value));
        new_value->params = (void *)worker;
        new_value->callback = callbackContinueWorkerFromWaitingPush;
        
        struct set_wait_list_key key = { object_id, offset, size, hash };
        
        GetsetInsertTagged(&set_wait_list, &key, new_value);
    }

    glbStatRemoteOutputRequests++;
    
    struct object_paths_value *obj = (void *)i64GetHashtable(&object_paths, object_id);
    if (obj == NULL || obj->local_id == -1)
    {
        // we need to find path
        log("Sending broadcast to find object=%lld\n", object_id);
        glbStatRemotePathMisses++;
        RequestObjectPathBroadcast(object_id, -1);
        // now, we can't resolve request
        return;
    }
    
    log("request sent [to %lld]\n", obj->local_id);
    // now, we know which hive handles object - request it from him
    struct hive_connection *connection = GetConnectionById(obj->local_id, NULL);

    BYTE header[8] = {API_PUSH_OBJECT, 24 + size, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    lock_write(&connection->lock);
    emitData(connection->outgoing, (char *) header, sizeof(header), 0);
    emitData(connection->outgoing, (char *)&object_id, 8, 0);
    emitData(connection->outgoing, (char *)&offset,    8, 0);
    emitData(connection->outgoing, (char *)&size,      8, 0);
    emitData(connection->outgoing, (char *) data,   size, 0);
    unlock_write(&connection->lock);
}

void StartNewWorkerRemote(struct hive_connection *con, int64_t worker_id, int64_t global_id, void *inputTable)
{
    glbStatRemoteOutputRequests++;
        
    con->queue_len++;
    log("Starting new REMOTE worker %lld [input table %p] [on local_id=%lld]\n", worker_id, inputTable, con->local_id);
    BYTE header[8] = {API_CALL_WORKER, 16 + Workers[worker_id].inputSize, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    lock_write(&con->lock);
    emitData(con->outgoing, (char *) header, sizeof(header), 0);
    emitData(con->outgoing, (char *)&worker_id, 8, 0);
    emitData(con->outgoing, (char *)&global_id, 8, 0);
    emitData(con->outgoing, (char *) inputTable, Workers[worker_id].inputSize, 0);
    unlock_write(&con->lock);
}

void SendHiveState()
{
    int64_t this_wait_list_len = wait_list_len;
    int64_t this_queue_len = glb_scheduler.len;
    // TODO: create better idle time getter
    int64_t this_idle_time = 0;
    
    log("sending hive state [%lld %lld %lld]\n", this_wait_list_len, this_queue_len, this_idle_time);

    BYTE message[8+24] = {API_GET_HIVE_STATE, 24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8+0,  &this_wait_list_len, 8);
    memcpy(message + 8+8,  &this_queue_len, 8);
    memcpy(message + 8+16, &this_idle_time, 8);
    lock_read(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL)
        {
            lock_write(&connections[i]->lock);
            emitData(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            unlock_write(&connections[i]->lock);
        }
    }
    unlock_read(&connections_lock);
}


/*---------------------------------------------- processes logic ---------------------------------------------*/

#ifdef _WIN32
static DWORD PagesAllocator(void *param)
#else
static void *PagesAllocator(void *param)
#endif
{
    (void)param;
    #ifdef SEQUENCE_PAGE_ALLOCATION
    int64_t next_page = 0;
    #endif
    while (1)
    {
        // check if there is more pages
        int64_t free_objects = 0;
        lock_read(&pages_lock);
        for (int64_t i = 0; i < pages_len; ++i)
        {
            free_objects += OBJECTS_PER_PAGE - pages[i].next_allocated_id;
        }
        unlock_read(&pages_lock);

        // buffer for ~1e6 allocation/second for 0.5 minute
        if (free_objects < OBJECTS_PER_PAGE * 2)
        {
            // request rendom page
            #ifdef SEQUENCE_PAGE_ALLOCATION
            int64_t page_id = next_page++;
            #else
            int64_t page_id = 0;
            SECURE_RANDOM((BYTE *)&page_id, 5);
            #endif
            RequestMemoryPage(page_id | 0x8000000000000000ULL);
        }

        Sleep(300);
    }
}


#ifdef _WIN32
static DWORD StateSender(void *param)
#else
static void *StateSender(void *param)
#endif
{
    (void)param;
    int64_t time = GetTicks();
    #define PAUSE_SIZE 50000
    while (1)
    {
        if (time - GetTicks() > MicrosecondsToTicks(PAUSE_SIZE))
        {
            time = GetTicks();
            SendHiveState();
        }
        #ifdef _WIN32
        Sleep(PAUSE_SIZE / 1000 + 1);
        #else
        usleep(PAUSE_SIZE);
        #endif
        #if BUFERIZATE == 1
            lock_read(&connections_lock);
            for (int64_t i = 0; i < connections_len; ++i)
            {
                if (connections[i]->outgoing.sock != INVALID_SOCKET)
                {
                    lock_write(&connections[i]->lock);
                    timedFlush(connections[i]->outgoing);
                    unlock_write(&connections[i]->lock);
                }
            }
            unlock_read(&connections_lock);
        #endif
    }
}


void start_remote_subsystem(int64_t noStdin) 
{
    log("intiializating network...\n");

    #ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    #else
    hEpoll = epoll_create1(0); 
    if (hEpoll == -1) {
        perror("epoll_create1 failed");
        exit(1);
    }
    signal(SIGPIPE, SIG_IGN);
    #endif

    char cmd[128] = {};
    int16_t port = 0;
    

    if (!noStdin)
    {
        print("[p <port>] to select port [c <ip> <port>] to connect remote [r] to confirm configuration\n");
        print("Enter command>");
        myScanS(cmd);
        
        if (cmd[0] == 'p' || cmd[0] == 'P')
        {
            port = myScanI64();
            print("Confirmed port=%lld\n", port);
            myScanS(cmd);
        }
    }
    

    for (int64_t i = 0; i < 2; ++i)
    {
        #ifdef _WIN32
        DWORD wkId;
        thread_t hwk = CreateThread(NULL, 0, Worker, NULL, 0, &wkId);
        (void)hwk;
        #else
        pthread_t wkId;
        pthread_create(&wkId, NULL, Worker, &port);
        (void)wkId;
        #endif
    }

    #ifdef _WIN32
    DWORD clId;
    thread_t hcl = CreateThread(NULL, 0, ConnectionListnerWorker, &port, 0, &clId);
    (void)hcl;
    #else
    pthread_t hcl;
    pthread_create(&hcl, NULL, ConnectionListnerWorker, &port);
    (void)hcl;
    #endif

    while (server_port == -1)
    {
        Sleep(1);
    }
    
    while (!noStdin) 
    {
        print("Enter command>");
        log("Get command [%s]\n", cmd);
        if (cmd[0] == 'c' || cmd[0] == 'C')
        {
            print("Selected connect command.\n");
            print("Enter IP and PORT.\n");
            char host[128];
            char port[128];
            myScanS(host);
            myScanS(port);
            print("Trying to connect to [%s] [%s]...\n", host, port);
            int64_t res = InitiateConnection(host, port);
            print("Temporary local_id = %lld\n", res);
        }
        else if (cmd[0] == 'r' || cmd[0] == 'R')
        {
            print("Configuration confirmed\n");
            break;
        }
        myScanS(cmd);
    }
    
    log("get id\n");

    lock_write(&ServerIdGetLock);

    /* try to get ID */
    while (!trylock_write(&ServerIdGetLock))
    {
        int64_t id = 0;
        SECURE_RANDOM((BYTE *)&id, 8);
        id |= 0x8000000000000000LL;
        RequestServerId(id);
        
        Sleep(50);
    }

    log("sleeping\n");

    Sleep(1000);
    
    DumpConnections();

    log("running threads\n");


    #ifdef _WIN32
    DWORD paId;
    HANDLE hpa = CreateThread(NULL, 0, PagesAllocator, &port, 0, &paId);
    (void)hpa;
    #else
    pthread_t hpa;
    pthread_create(&hpa, NULL, PagesAllocator, &port);
    (void)hpa;
    #endif



    #ifdef _WIN32    
    DWORD ssId;
    HANDLE hss = CreateThread(NULL, 0, StateSender, &port, 0, &ssId);
    (void)hss;
    #else
    pthread_t hss;
    pthread_create(&hss, NULL, StateSender, &port);
    (void)hss;
    #endif
    

    log("network initializated\n");
}

// TODO: clean_remote_subsystem()
// WSACleanup();

