#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "runtime.h"
#include "remote.h"
#include "providers.h"

SRWLOCK ServerIdGetLock = SRWLOCK_INIT;

_Atomic int64_t next_local_id = 0;
int64_t server_port = -1;
_Atomic int64_t thisServerId = -1;
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
void SendIDConfirm(struct hive_connection *con, BYTE *broadcast_id);
void ConfirmConnection(struct hive_connection *con, int64_t local_id, int64_t port);
int64_t RedirectBroadcastQuery(int64_t page_id, BYTE *broadcast_id, int64_t except_this_local_id);
int64_t RedirectBroadcastIDQuery(int64_t want_id, BYTE *broadcast_id, int64_t except_this_local_id);
void RequestObjectPathBroadcast(int64_t object, int64_t except_this_local_id);
void AnswerRequestObjectPath(int64_t object, int64_t distance);
void AnswerQueryObject(struct hive_connection *con, void *shifted_buffer, int64_t object_id, int64_t offset, int64_t size);
void AnswerPushObject(struct hive_connection *con, int64_t object_id, int64_t offset, int64_t size);
void RequestPathToIDBroadcast(int64_t global_id, int64_t except_this_local_id);
void AnswerRequestPathToID(int64_t global_id, int64_t distance);


void ConfirmPage(int64_t page_id)
{
    log("!!!!! >>>>>> Page allocation id=%lld confirmed\n", page_id);
    
    AcquireSRWLockExclusive(&pages_lock);
    pages[pages_len++] = (struct memory_page){page_id, 0, 0};
    ReleaseSRWLockExclusive(&pages_lock);
}

void ConfirmID(int64_t confirmed_id)
{
    if (thisServerId == -1)
    {
        print("Server id=%lld confirmed\n", confirmed_id);
        thisServerId = confirmed_id;
        ReleaseSRWLockExclusive(&ServerIdGetLock);
    }
}


void HandleNewConnection(SOCKET client, SOCKADDR_STORAGE storage, int storage_len)
{
    log("Client connected\n");
    
    struct hive_connection *new_connection = myMalloc(sizeof(*new_connection));
    struct connection_context *new_context = myMalloc(sizeof(*new_context));

    // initializate new connection
    new_connection->lock = (SRWLOCK)SRWLOCK_INIT;
    new_connection->outgoing = INVALID_SOCKET;
    new_connection->local_id = next_local_id++;
    new_connection->address = storage;
    new_connection->address_len = storage_len;
    new_connection->ctx = new_context;
    new_connection->wait_list_len = INT_INFINITY;
    new_connection->queue_len = INT_INFINITY;
    new_connection->idle_time = 0;

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

void UpdateWaitingPush(int64_t object_id, int64_t offset, int64_t size)
{
    /* for each local_id requesting this query - answer */
    AcquireSRWLockShared(&push_requests.lock);
    struct query_object_request key = { object_id, offset, size, NULL };
    struct query_object_request *p = (void *)GetHashtableNoLock(&push_requests, (BYTE *)&key, PUSH_HASHING_BYTES, 0);
    struct linked_node *cur = NULL;
    if (p != NULL)
    {
        cur = p->local_ids;
        p->local_ids = NULL;
    }
    ReleaseSRWLockShared(&push_requests.lock);

    /* redirect answer */
    while (cur)
    {
        AnswerPushObject(GetConnectionById(cur->local_id, NULL), object_id, offset, size);
        void *tmp = cur;
        cur = cur->next;
        myFree(tmp);
    }

    /* update wait list */
    AcquireSRWLockExclusive(&wait_list_lock);
    for (int64_t i = 0; i < wait_list_len; ++i)
    {
        struct waiting_worker *w = wait_list[i];
        if (w->waiting_data->type == WAITING_PUSH)
        {
            struct waiting_push *cause = (struct waiting_push *)w->waiting_data;
            if (cause->object_id == object_id && myAbs(cause->size) == size && cause->offset == offset)
            {
                EnqueueWorkerFromWaitList(w, 0);
                myFree(cause->data);
                myFree(cause);
                myFree(w);
                wait_list[i] = wait_list[--wait_list_len];
                i--;
            }
        }
    }
    ReleaseSRWLockExclusive(&wait_list_lock);
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
            27 byte: push ID [if API_ANSWER_PUSH_PIPE]

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
            AcquireSRWLockExclusive(&connections_lock);
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
                    ReleaseSRWLockExclusive(&connections_lock);
                    return 1;
                }
            }
            print("Error: API_ANSWER_REQUEST_CONNECTION becouse there is no connection with local_id=%lld\n", local_id);
            ReleaseSRWLockExclusive(&connections_lock);
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
            
            AcquireSRWLockExclusive(&known_id_broadcasts.lock);            
            for (int64_t i = 0; i < known_id_broadcasts.alloc; ++i)
            {
                struct hashtable_node *cur = known_id_broadcasts.table[i];
                while (cur != NULL)
                {
                    if (!equal_bytes(cur->bytes, broadcast_id, BROADCAST_ID_LENGTH) &&
                        ((struct id_request *)cur->id)->id == want_id)
                    {
                        // refuse
                        log("Refuse id\n");
                        ReleaseSRWLockExclusive(&known_id_broadcasts.lock);
                        return 1;
                    }
                    cur = cur->next;
                }
            }
            // check - if we already answering this broadcast
            struct id_request *broadcast = (struct id_request *)GetHashtableNoLock(&known_id_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 0);
            if (broadcast == NULL)
            {
                // create local broadcast entry
                broadcast = myMalloc(sizeof(*broadcast));
                broadcast->id = want_id;
                broadcast->local_redirect_id = con->local_id;
                broadcast->answered = 0;
                broadcast->requested = 0;
                SetHashtableNoLock(&known_id_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, (int64_t)broadcast);

                // redirect queries
                broadcast->requested = RedirectBroadcastIDQuery(want_id, broadcast_id, con->local_id);
            }
            else
            {
                if (broadcast->local_redirect_id != con->local_id)
                {
                    /* simply answer yes */
                    log("Confirmed becouse local_id differs\n");
                    SendIDConfirm(con, broadcast_id);
                }
            }

            int64_t answered = broadcast->answered;
            int64_t requested = broadcast->requested;
            ReleaseSRWLockExclusive(&known_id_broadcasts.lock);

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
            AcquireSRWLockShared(&known_id_broadcasts.lock);
            struct id_request *broadcast = (struct id_request *)GetHashtableNoLock(&known_id_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 0);
            if (broadcast != NULL)
            {
                int64_t new_count = ++broadcast->answered;
                if (new_count == broadcast->requested) // answer query
                {
                    log("Broadcast confirmed\n");
                    if (broadcast->local_redirect_id == -1)
                    {
                        // request confirmed
                        // all is ok - start working server
                        ConfirmID(broadcast->id);
                    }
                    else
                    {
                        AcquireSRWLockShared(&connections_lock);
                        struct hive_connection *ansCon = GetConnectionById(broadcast->local_redirect_id, NULL);
                        ReleaseSRWLockShared(&connections_lock);
                        if (ansCon)
                        {
                            SendIDConfirm(ansCon, broadcast_id);
                        }
                    }
                }
            }
            ReleaseSRWLockShared(&known_id_broadcasts.lock);
            return 1;
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
            // TODO: change to check only this server created broadcasts, to not refuse if same page trying to allocate twice from one server
            // TODO: maybe use Shared access here, and only if page isn't requested - use exclusive?
            AcquireSRWLockExclusive(&known_page_broadcasts.lock);
            
            for (int64_t i = 0; i < known_page_broadcasts.alloc; ++i)
            {
                struct hashtable_node *cur = known_page_broadcasts.table[i];
                while (cur != NULL)
                {
                    if (!equal_bytes(cur->bytes, broadcast_id, BROADCAST_ID_LENGTH) &&
                        ((struct memory_page_request *)cur->id)->page_id == page_id)
                    {
                        // refuse
                        log("Refuse allocation\n");
                        ReleaseSRWLockExclusive(&known_page_broadcasts.lock);
                        return 1;
                    }
                    cur = cur->next;
                }
            }
            
            // check - if we already answering this broadcast
            struct memory_page_request *broadcast = (struct memory_page_request *)GetHashtableNoLock(&known_page_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 0);
            if (broadcast == NULL)
            {
                // create local broadcast entry
                broadcast = myMalloc(sizeof(*broadcast));
                broadcast->page_id = page_id;
                broadcast->local_redirect_id = con->local_id;
                broadcast->answered = 0;
                broadcast->requested = 0;
                SetHashtableNoLock(&known_page_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, (int64_t)broadcast);

                // redirect queries
                broadcast->requested = RedirectBroadcastQuery(page_id, broadcast_id, con->local_id);
            }
            else
            {
                if (broadcast->local_redirect_id != con->local_id)
                {
                    /* simply answer yes */
                    log("Confirmed becouse local_id differs\n");
                    SendPageAllocationConfirm(con, broadcast_id);
                }
            }

            int64_t answered = broadcast->answered;
            int64_t requested = broadcast->requested;
            ReleaseSRWLockExclusive(&known_page_broadcasts.lock);

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
            AcquireSRWLockShared(&known_page_broadcasts.lock);
            struct memory_page_request *broadcast = (struct memory_page_request *)GetHashtableNoLock(&known_page_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 0);
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
            ReleaseSRWLockShared(&known_page_broadcasts.lock);
            return 1;
        }
        case API_QUERY_OBJECT:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t query_offset = *(int64_t *)(ctx->res_buffer+8);
            int64_t query_size = *(int64_t *)(ctx->res_buffer+16);
            /* if this is local object - answer query */
            struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
            if (obj != NULL)
            {
                AnswerQueryObject(con, (BYTE *)obj + query_offset, object_id, query_offset, query_size);
                return 1;
            }
            /* else - add request to global request hashtable */
            struct linked_node *new_node = myMalloc(sizeof(*new_node));
            new_node->local_id = con->local_id;
            
            AcquireSRWLockExclusive(&query_requests.lock);
            
            struct query_object_request key = { object_id, query_offset, query_size, NULL };
            struct query_object_request *q = (void *)GetHashtableNoLock(&query_requests, (BYTE *)&key, QUERY_HASHING_BYTES, 0);
            if (q == NULL)
            {
                q = myMalloc(sizeof(*q));
                *q = key;
                SetHashtableNoLock(&query_requests, (BYTE *)q, QUERY_HASHING_BYTES, (int64_t)q);
            }
            /* update q's linked list */
            new_node->next = q->local_ids;
            q->local_ids = new_node;
            
            ReleaseSRWLockExclusive(&query_requests.lock);
            
            return 1;
        }
        case API_ANSWER_QUERY_OBJECT:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t query_offset = *(int64_t *)(ctx->res_buffer+8);
            int64_t query_size = *(int64_t *)(ctx->res_buffer+16);
            BYTE *data = ctx->res_buffer+24;

            /* for each local_id requesting this query - answer */
            AcquireSRWLockExclusive(&query_requests.lock);
            struct query_object_request key = { object_id, query_offset, query_size, NULL };
            struct query_object_request *q = (void *)GetHashtableNoLock(&query_requests, (BYTE *)&key, QUERY_HASHING_BYTES, 0);
            struct linked_node *cur = NULL;
            if (q != NULL)
            {
                cur = q->local_ids;
                q->local_ids = NULL;
            }
            ReleaseSRWLockExclusive(&query_requests.lock);

            /* redirect answer */
            while (cur)
            {
                AnswerQueryObject(GetConnectionById(cur->local_id, NULL), data, object_id, query_offset, query_size);
                void *tmp = cur;
                cur = cur->next;
                myFree(tmp);
            }
            
            /* for each program in waiting list - continue if this is it's request */
            AcquireSRWLockExclusive(&wait_list_lock);
            for (int64_t i = 0; i < wait_list_len; ++i)
            {
                struct waiting_worker *w = wait_list[i];
                if (w->waiting_data->type == WAITING_QUERY)
                {
                    struct waiting_query *cause = (struct waiting_query *)w->waiting_data;
                    if (cause->object_id == object_id && myAbs(cause->size) == query_size && cause->offset == query_offset)
                    {
                        int64_t rdiValue = 0;
                        UpdateFromQueryResult(cause->destination, object_id, query_offset, query_size, data, &rdiValue);
                        EnqueueWorkerFromWaitList(w, rdiValue);
                        myFree(cause);
                        myFree(w);
                        wait_list[i] = wait_list[--wait_list_len];
                        i--;
                    }
                }
            }
            ReleaseSRWLockExclusive(&wait_list_lock);
            
            return 1;
        }
        case API_PUSH_OBJECT:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t offset = *(int64_t *)(ctx->res_buffer+8);
            int64_t size = *(int64_t *)(ctx->res_buffer+16);
            BYTE *data = ctx->res_buffer + 24;
            
            log("get push object %lld\n", object_id);
            
            /* if this is local object - answer */
            struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
            if (obj != NULL)
            {
                log("local-pushed\n");
                universalUpdateLocalPush(obj, offset, size, data);
                AnswerPushObject(con, object_id, offset, size);
                UpdateWaitingPush(object_id, offset, size);
                return 1;
            }
            log("remote-redirected\n");
            
            /* else - add request to global request hashtable */
            struct linked_node *new_node = myMalloc(sizeof(*new_node));
            new_node->local_id = con->local_id;
            
            AcquireSRWLockExclusive(&push_requests.lock);
            struct push_object_request key = { object_id, offset, size, NULL };
            struct push_object_request *p = (void *)GetHashtableNoLock(&push_requests, (BYTE *)&key, PUSH_HASHING_BYTES, 0);
            if (p == NULL)
            {
                p = myMalloc(sizeof(*p));
                *p = key;
                SetHashtableNoLock(&push_requests, (BYTE *)p, PUSH_HASHING_BYTES, (int64_t)p);
            }
            /* update p's linked list */
            new_node->next = p->local_ids;
            p->local_ids = new_node;
            ReleaseSRWLockExclusive(&push_requests.lock);
            
            /* and forward this push futher */
            // TODO: store data in some place, and repeat call manually after some time
            //       [now this makes server who want this push...]
            RequestObjectSet(object_id, offset, size, data);
            
            return 1;
        }
        case API_ANSWER_PUSH_OBJECT:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t offset = *(int64_t *)(ctx->res_buffer+8);
            int64_t size = *(int64_t *)(ctx->res_buffer+16);
            
            /* for each program in waiting list - continue if this is it's request */
            /* for each waiting local_id - answer */
            UpdateWaitingPush(object_id, offset, size);
            
            return 1;
        }
        case API_REQUEST_PATH:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            BYTE *broadcast_id = ctx->res_buffer + 8;
            /* if object is our - send answers */
            void *obj = (void *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
            if (obj != NULL)
            {
                /* send answers */
                AnswerRequestObjectPath(object_id, 1);
                return 1;
            }
            /* else - if broadcast is new, redirect it */
            AcquireSRWLockExclusive(&known_path_broadcasts.lock);
            if (GetHashtableNoLock(&known_path_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 0) == 0)
            {
                RequestObjectPathBroadcast(object_id, con->local_id);
                SetHashtableNoLock(&known_path_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 1);
            }
            ReleaseSRWLockExclusive(&known_path_broadcasts.lock);
            return 1;
        }
        case API_ANSWER_REQUEST_PATH:
        {
            int64_t object_id = *(int64_t *)(ctx->res_buffer);
            int64_t distance = *(int64_t *)(ctx->res_buffer + 8);
            /* get object - if it is local - don't update anything, and don't send answers */
            void *loc_obj = (void *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
            if (loc_obj != NULL)
            {
                return 1;
            }
            /* update known objects base */
            AcquireSRWLockExclusive(&known_objects.lock);
            struct known_object *obj = (void *)GetHashtableNoLock(&known_objects, (BYTE *)&object_id, 8, 0);
            if (obj != NULL && obj->distance > distance)
            {
                log("UPDATE PATH 1 TO %lld [distance=%lld, local_id=%lld]\n", object_id, distance, con->local_id);
                obj->local_id = con->local_id;
                obj->distance = distance;
                SetHashtableNoLock(&known_objects, (BYTE *)&object_id, 8, (int64_t)obj);
                ReleaseSRWLockExclusive(&known_objects.lock);
                AnswerRequestObjectPath(object_id, distance + 1);
                return 1;
            }
            else if (obj == NULL)
            {
                log("UPDATE PATH 2 TO %lld [distance=%lld, local_id=%lld]\n", object_id, distance, con->local_id);
                obj = myMalloc(sizeof(*obj));
                obj->local_id = con->local_id;
                obj->distance = distance + 1;
                SetHashtableNoLock(&known_objects, (BYTE *)&object_id, 8, (int64_t)obj);
                ReleaseSRWLockExclusive(&known_objects.lock);
                AnswerRequestObjectPath(object_id, distance + 1);
                return 1;
            }
            ReleaseSRWLockExclusive(&known_objects.lock);
            return 1;
        }
        case API_REQUEST_PATH_TO_ID:
        {
            int64_t global_id = *(int64_t *)(ctx->res_buffer);
            BYTE *broadcast_id = ctx->res_buffer + 8;
            /* if object is our - send answers */
            if (global_id == thisServerId)
            {
                AnswerRequestPathToID(global_id, 1);
                return 1;
            }
            /* else - if broadcast is new, redirect it */
            AcquireSRWLockExclusive(&known_path_id_broadcasts.lock);
            if (GetHashtableNoLock(&known_path_id_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 0) == 0)
            {
                RequestPathToIDBroadcast(global_id, con->local_id);
                SetHashtableNoLock(&known_path_id_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, 1);
            }
            ReleaseSRWLockExclusive(&known_path_id_broadcasts.lock);
            return 1;
        }
        case API_ANSWER_REQUEST_PATH_TO_ID:
        {
            int64_t global_id = *(int64_t *)(ctx->res_buffer);
            int64_t distance = *(int64_t *)(ctx->res_buffer + 8);
            /* get object - if it is local - don't update anything, and don't send answers */
            if (global_id == thisServerId)
            {
                return 1;
            }
            /* update known objects base */
            AcquireSRWLockExclusive(&known_hives.lock);
            struct known_hive *obj = (void *)GetHashtableNoLock(&known_hives, (BYTE *)&global_id, 8, 0);
            if (obj != NULL && obj->distance > distance)
            {
                log("UPDATE ID PATH 1 TO %lld [distance=%lld, local_id=%lld]\n", global_id, distance, con->local_id);
                obj->local_id = con->local_id;
                obj->distance = distance;
                SetHashtableNoLock(&known_hives, (BYTE *)&global_id, 8, (int64_t)obj);
                ReleaseSRWLockExclusive(&known_hives.lock);
                AnswerRequestPathToID(global_id, distance + 1);
                return 1;
            }
            else if (obj == NULL)
            {
                log("UPDATE ID PATH 2 TO %lld [distance=%lld, local_id=%lld]\n", global_id, distance, con->local_id);
                obj = myMalloc(sizeof(*obj));
                obj->local_id = con->local_id;
                obj->distance = distance + 1;
                SetHashtableNoLock(&known_hives, (BYTE *)&global_id, 8, (int64_t)obj);
                ReleaseSRWLockExclusive(&known_hives.lock);
                AnswerRequestPathToID(global_id, distance + 1);
                return 1;
            }
            ReleaseSRWLockExclusive(&known_hives.lock);
            return 1;
        }
        case API_CALL_WORKER:
        {
            int64_t worker_id = *(int64_t *)(ctx->res_buffer+0);
            int64_t global_id = *(int64_t *)(ctx->res_buffer+8);
            BYTE *data = ctx->res_buffer + 16;
            log("Get remote start worker %lld from local_id=%lld on %lld\n", worker_id, con->local_id, global_id);
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
            print("[NETWORK]: Peer disconnected\n");
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
                        print("closing connection...\n");
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
    AcquireSRWLockExclusive(&con->lock);
    send(con->outgoing, (char *)message, sizeof(message), 0);
    ReleaseSRWLockExclusive(&con->lock);
}

void SendIDConfirm(struct hive_connection *con, BYTE *broadcast_id)
{
    log("Send confirmation of page allocation to local_id=%lld\n", con->local_id);
    BYTE message[8+27] = {API_ANSWER_REQUEST_ID, BROADCAST_ID_LENGTH, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8, broadcast_id, BROADCAST_ID_LENGTH);
    AcquireSRWLockExclusive(&con->lock);
    send(con->outgoing, (char *)message, sizeof(message), 0);
    ReleaseSRWLockExclusive(&con->lock);
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
            log("Redirecting page query to local_id=%lld\n", connections[i]->local_id);
            AcquireSRWLockExclusive(&connections[i]->lock);
            send(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            ReleaseSRWLockExclusive(&connections[i]->lock);
            send_cnt++;
        }
    }
    ReleaseSRWLockShared(&connections_lock);
    return send_cnt;
}


int64_t RedirectBroadcastIDQuery(int64_t want_id, BYTE *broadcast_id, int64_t except_this_local_id)
{
    AcquireSRWLockShared(&connections_lock);
    BYTE message[8+8+27] = {API_REQUEST_ID, 8+27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8,  &want_id, 8);
    memcpy(message + 8+8, broadcast_id, BROADCAST_ID_LENGTH);
    int64_t send_cnt = 0;
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id != except_this_local_id)
        {
            log("Redirecting id query to local_id=%lld\n", connections[i]->local_id);
            AcquireSRWLockExclusive(&connections[i]->lock);
            send(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            ReleaseSRWLockExclusive(&connections[i]->lock);
            send_cnt++;
        }
    }
    ReleaseSRWLockShared(&connections_lock);
    return send_cnt;
}


void ConfirmConnection(struct hive_connection *ctx, int64_t reply_id, int64_t port)
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
        log("Connection is successful! [1]\n");
        AcquireSRWLockShared(&connections_lock);
        ctx->outgoing = s;
        ReleaseSRWLockShared(&connections_lock);

        BYTE header[8] = {0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send(s, (char *) header, sizeof(header), 0);
        send(s, (char *)&reply_id, 8, 0);
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
        log("Connection is successful! [2]\n");
        
        AcquireSRWLockExclusive(&connections_lock);
        
        int64_t local_id = next_local_id++;
        int64_t conn = connections_len++;
        
        connections[conn] = myMalloc(sizeof(**connections));
        connections[conn]->lock = (SRWLOCK)SRWLOCK_INIT;
        connections[conn]->ctx = NULL;
        connections[conn]->outgoing = s;
        connections[conn]->local_id = local_id;
        
        connections[conn]->wait_list_len = INT_INFINITY;
        connections[conn]->queue_len = INT_INFINITY;
        connections[conn]->idle_time = 0;
        
        ReleaseSRWLockExclusive(&connections_lock);
        
        BYTE header[8] = {API_REQUEST_CONNECTION, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send(s, (char *) header, sizeof(header), 0);
        send(s, (char *)&local_id, 8, 0);
        send(s, (char *)&server_port, 8, 0);
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


    // create random seed
    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    BCryptGenRandom(NULL, broadcast_id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    
    AcquireSRWLockExclusive(&known_page_broadcasts.lock);

    // request page from all neibours
    struct memory_page_request *broadcast = myMalloc(sizeof(*broadcast));
    broadcast->page_id = page_id;
    broadcast->local_redirect_id = -1; // this hive
    broadcast->answered = 0;
    broadcast->requested = 0;
    SetHashtableNoLock(&known_page_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, (int64_t)broadcast);
    
    log("Created broadcast with prefix=%llx\n", *(int64_t *)broadcast_id);

    // redirect queries
    broadcast->requested = RedirectBroadcastQuery(page_id, broadcast_id, -1);

    int64_t requested = broadcast->requested;
    ReleaseSRWLockExclusive(&known_page_broadcasts.lock);

    if (requested == 0)
    {
        // confirm page alloaction
        ConfirmPage(page_id);
    }
}

void RequestServerId(int64_t new_id)
{
    print("Trying to get server id %lld\n", new_id);

    // create random seed
    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    BCryptGenRandom(NULL, broadcast_id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    
    AcquireSRWLockExclusive(&known_id_broadcasts.lock);

    struct id_request *broadcast = myMalloc(sizeof(*broadcast));
    broadcast->id = new_id;
    broadcast->local_redirect_id = -1; // this hive
    broadcast->answered = 0;
    broadcast->requested = 0;
    SetHashtableNoLock(&known_id_broadcasts, broadcast_id, BROADCAST_ID_LENGTH, (int64_t)broadcast);

    log("Created broadcast with prefix=%llx\n", *(int64_t *)broadcast_id);

    // redirect queries
    broadcast->requested = RedirectBroadcastIDQuery(new_id, broadcast_id, -1);
    int64_t requested = broadcast->requested;
    ReleaseSRWLockExclusive(&known_id_broadcasts.lock);
    if (requested == 0)
    {
        ConfirmID(new_id);
    }
}


void AnswerPushObject(struct hive_connection *con, int64_t object_id, int64_t offset, int64_t size)
{    
    log("Answer Push Object to localid=%lld [%lld+%lld:%lld]\n", con->local_id, object_id, offset, size);
    BYTE header[8] = {API_ANSWER_PUSH_OBJECT, 24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    AcquireSRWLockExclusive(&con->lock);
    send(con->outgoing, (char *) header, sizeof(header), 0);
    send(con->outgoing, (char *)&object_id, 8, 0);
    send(con->outgoing, (char *)&offset, 8, 0);
    send(con->outgoing, (char *)&size,   8, 0);
    ReleaseSRWLockExclusive(&con->lock);
}

void AnswerQueryObject(struct hive_connection *con, void *shifted_buffer, int64_t object_id, int64_t offset, int64_t size)
{    
    log("Answer Query Object to localid=%lld [%lld+%lld:%lld]\n", con->local_id, object_id, offset, size);
    BYTE header[8] = {API_ANSWER_QUERY_OBJECT, 24+size, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    AcquireSRWLockExclusive(&con->lock);
    send(con->outgoing, (char *) header, sizeof(header), 0);
    send(con->outgoing, (char *)&object_id, 8, 0);
    send(con->outgoing, (char *)&offset, 8, 0);
    send(con->outgoing, (char *)&size,   8, 0);
    // send data
    send(con->outgoing, (char *)shifted_buffer, size, 0);
    ReleaseSRWLockExclusive(&con->lock);
}
                

void AnswerRequestObjectPath(int64_t object, int64_t distance)
{
    BYTE message[8+16] = {API_ANSWER_REQUEST_PATH, 16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8, &object, 8);
    memcpy(message + 16, &distance, 8);
    AcquireSRWLockShared(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL)
        {
            log("send broadcast answer path to local_id=%lld\n", connections[i]->local_id);
            AcquireSRWLockExclusive(&connections[i]->lock);
            send(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            ReleaseSRWLockExclusive(&connections[i]->lock);
        }
    }
    ReleaseSRWLockShared(&connections_lock);
}


void RequestObjectPathBroadcast(int64_t object, int64_t except_this_local_id)
{
    // update known_object structure
    AcquireSRWLockExclusive(&known_objects.lock);
    struct known_object *obj = (void *)GetHashtableNoLock(&known_objects, (BYTE *)&object, 8, 0);
    if (obj == NULL)
    {
        obj = myMalloc(sizeof(*obj));
        obj->distance = INFINITY_DISTANCE;
        obj->local_id = -1;
    }
    else
    {
        obj->distance = INFINITY_DISTANCE;
    }
    ReleaseSRWLockExclusive(&known_objects.lock);

    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    BCryptGenRandom(NULL, broadcast_id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    BYTE message[8+8+27] = {API_REQUEST_PATH, 8+27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8,  &object, 8);
    memcpy(message + 8+8, broadcast_id, BROADCAST_ID_LENGTH);
    AcquireSRWLockShared(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id != except_this_local_id)
        {
            log("send broadcast path request to local_id=%lld\n", connections[i]->local_id);
            AcquireSRWLockExclusive(&connections[i]->lock);
            send(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            ReleaseSRWLockExclusive(&connections[i]->lock);
        }
    }
    ReleaseSRWLockShared(&connections_lock);
}



void AnswerRequestPathToID(int64_t global_id, int64_t distance)
{
    BYTE message[8+16] = {API_ANSWER_REQUEST_PATH, 16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8, &global_id, 8);
    memcpy(message + 16, &distance, 8);
    AcquireSRWLockShared(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL)
        {
            log("send broadcast answer id path to local_id=%lld\n", connections[i]->local_id);
            AcquireSRWLockExclusive(&connections[i]->lock);
            send(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            ReleaseSRWLockExclusive(&connections[i]->lock);
        }
    }
    ReleaseSRWLockShared(&connections_lock);
}


void RequestPathToIDBroadcast(int64_t global_id, int64_t except_this_local_id)
{
    // update known_object structure
    AcquireSRWLockExclusive(&known_hives.lock);
    struct known_object *obj = (void *)GetHashtableNoLock(&known_hives, (BYTE *)&global_id, 8, 0);
    if (obj == NULL)
    {
        obj = myMalloc(sizeof(*obj));
        obj->distance = INFINITY_DISTANCE;
        obj->local_id = -1;
    }
    else
    {
        obj->distance = INFINITY_DISTANCE;
    }
    ReleaseSRWLockExclusive(&known_hives.lock);

    BYTE broadcast_id[BROADCAST_ID_LENGTH];
    BCryptGenRandom(NULL, broadcast_id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    BYTE message[8+8+27] = {API_REQUEST_PATH, 8+27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8,  &global_id, 8);
    memcpy(message + 8+8, broadcast_id, BROADCAST_ID_LENGTH);
    AcquireSRWLockShared(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL && connections[i]->local_id != except_this_local_id)
        {
            log("send broadcast path request to local_id=%lld\n", connections[i]->local_id);
            AcquireSRWLockExclusive(&connections[i]->lock);
            send(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            ReleaseSRWLockExclusive(&connections[i]->lock);
        }
    }
    ReleaseSRWLockShared(&connections_lock);
}


void RequestObjectGet(int64_t object, int64_t offset, int64_t size)
{
    // find object in object table
    struct known_object *obj = (void *)GetHashtable(&known_objects, (BYTE *)&object, 8, 0);
    if (obj == NULL || obj->local_id == -1)
    {
        // we need to find path
        log("Sending broadcast to find object=%lld\n", object);
        RequestObjectPathBroadcast(object, -1);
        // now, we can't resolve request
        return;
    }
    // now, we know which hive handles object - request it from him
    struct hive_connection *connection = GetConnectionById(obj->local_id, NULL);

    BYTE header[8] = {API_QUERY_OBJECT, 24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    AcquireSRWLockExclusive(&connection->lock);
    send(connection->outgoing, (char *) header, sizeof(header), 0);
    send(connection->outgoing, (char *)&object, 8, 0);
    send(connection->outgoing, (char *)&offset, 8, 0);
    send(connection->outgoing, (char *)&size,   8, 0);
    ReleaseSRWLockExclusive(&connection->lock);
}

void RequestObjectSet(int64_t object_id, int64_t offset, int64_t size, void *data)
{
    log("request set object=%lld\n", object_id);
    // if this is local object - simply set it and answer [to who?]
    struct object *loc = (void *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
    if (loc != NULL)
    {
        log("local object - answer\n");
        universalUpdateLocalPush(loc, offset, size, data);
        /* update all local waiting processes */
        UpdateWaitingPush(object_id, offset, size);
        // it was found
        return;
    }
    // find object in object table
    struct known_object *obj = (void *)GetHashtable(&known_objects, (BYTE *)&object_id, 8, 0);
    if (obj == NULL || obj->local_id == -1)
    {
        log("don't know path\n");
        // we need to find path
        log("Sending broadcast to find object=%lld\n", object_id);
        RequestObjectPathBroadcast(object_id, -1);
        // now, we can't resolve request
        return;
    }
    
    log("request sent [to %lld]\n", obj->local_id);
    // now, we know which hive handles object - request it from him
    struct hive_connection *connection = GetConnectionById(obj->local_id, NULL);

    BYTE header[8] = {API_PUSH_OBJECT, 24 + size, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    AcquireSRWLockExclusive(&connection->lock);
    send(connection->outgoing, (char *) header, sizeof(header), 0);
    send(connection->outgoing, (char *)&object_id, 8, 0);
    send(connection->outgoing, (char *)&offset,    8, 0);
    send(connection->outgoing, (char *)&size,      8, 0);
    send(connection->outgoing, (char *) data,   size, 0);
    ReleaseSRWLockExclusive(&connection->lock);
}

void StartNewWorkerRemote(struct hive_connection *con, int64_t worker_id, int64_t global_id, void *inputTable)
{
    con->queue_len++;
    log("Starting new REMOTE worker %lld [input table %p] [on local_id=%lld]\n", worker_id, inputTable, con->local_id);
    BYTE header[8] = {API_CALL_WORKER, 16 + Workers[worker_id].inputSize, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    AcquireSRWLockExclusive(&con->lock);
    send(con->outgoing, (char *) header, sizeof(header), 0);
    send(con->outgoing, (char *)&worker_id, 8, 0);
    send(con->outgoing, (char *)&global_id, 8, 0);
    send(con->outgoing, (char *) inputTable, Workers[worker_id].inputSize, 0);
    ReleaseSRWLockExclusive(&con->lock);
}

void SendHiveState()
{
    AcquireSRWLockShared(&wait_list_lock);
    int64_t this_wait_list_len = wait_list_len;
    ReleaseSRWLockShared(&wait_list_lock);
    AcquireSRWLockShared(&queue_lock);
    int64_t this_queue_len = queue_len;
    ReleaseSRWLockShared(&queue_lock);
    // TODO: create better idle time getter
    int64_t this_idle_time = 0;
    
    log("sending hive state [%lld %lld %lld]\n", this_wait_list_len, this_queue_len, this_idle_time);

    BYTE message[8+24] = {API_GET_HIVE_STATE, 24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(message + 8+0,  &this_wait_list_len, 8);
    memcpy(message + 8+8,  &this_queue_len, 8);
    memcpy(message + 8+16, &this_idle_time, 8);
    AcquireSRWLockShared(&connections_lock);
    for (int64_t i = 0; i < connections_len; ++i)
    {
        if (connections[i]->ctx != NULL)
        {
            AcquireSRWLockExclusive(&connections[i]->lock);
            send(connections[i]->outgoing, (char *)message, sizeof(message), 0);
            ReleaseSRWLockExclusive(&connections[i]->lock);
        }
    }
    ReleaseSRWLockShared(&connections_lock);
}


/*---------------------------------------------- processes logic ---------------------------------------------*/

static DWORD PagesAllocator(void *param)
{
    (void)param;
    #ifdef SEQUENCE_PAGE_ALLOCATION
    int64_t next_page = 0;
    #endif
    while (1)
    {
        // check if there is more pages
        int64_t free_objects = 0;
        AcquireSRWLockShared(&pages_lock);
        for (int64_t i = 0; i < pages_len; ++i)
        {
            free_objects += OBJECTS_PER_PAGE - pages[i].next_allocated_id;
        }
        ReleaseSRWLockShared(&pages_lock);

        // buffer for ~1e6 allocation/second for 0.5 minute
        if (free_objects < OBJECTS_PER_PAGE * 2)
        {
            // request rendom page
            #ifdef SEQUENCE_PAGE_ALLOCATION
            int64_t page_id = next_page++;
            #else
            int64_t page_id = 0;
            BCryptGenRandom(NULL, (BYTE *)&page_id, 5, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            #endif
            RequestMemoryPage(page_id | 0x8000000000000000ULL);
        }
        
        Sleep(300);
    }
}


static DWORD StateSender(void *param)
{
    (void)param;
    while (1)
    {
        SendHiveState();
        Sleep(50);
    }
}


void start_remote_subsystem() 
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    char cmd[128] = {};
    myScanS(cmd);

    int16_t port = 0;
    
    if (cmd[0] == 'p' || cmd[0] == 'P')
    {
        port = myScanI64();
        print("Confirmed port=%lld\n", port);
        myScanS(cmd);
    }

    for (int64_t i = 0; i < 2; ++i)
    {
        DWORD wkId;
        HANDLE hwk = CreateThread(NULL, 0, Worker, NULL, 0, &wkId);
        (void)hwk;
    }

    DWORD clId;
    HANDLE hcl = CreateThread(NULL, 0, ConnectionListnerWorker, &port, 0, &clId);
    (void)hcl;

    while (server_port == -1)
    {
        Sleep(1);
    }
    
    while (1) 
    {
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

    AcquireSRWLockExclusive(&ServerIdGetLock);

    /* try to get ID */
    while (!TryAcquireSRWLockExclusive(&ServerIdGetLock))
    {
        int64_t id = 0;
        BCryptGenRandom(NULL, (BYTE *)&id, 8, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        id |= 0x8000000000000000LL;
        RequestServerId(id);
        Sleep(50);
    }

    Sleep(1000);
    DumpConnections();

    
    DWORD paId;
    HANDLE hpa = CreateThread(NULL, 0, PagesAllocator, &port, 0, &paId);
    (void)hpa;
    
    DWORD ssId;
    HANDLE hss = CreateThread(NULL, 0, StateSender, &port, 0, &ssId);
    (void)hss;
}

// TODO: clean_remote_subsystem()
// WSACleanup();
