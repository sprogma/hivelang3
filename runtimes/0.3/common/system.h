#ifndef SYSTEM_H
#define SYSTEM_H


// headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <sys/socket.h>
    #include <sys/epoll.h>
    #include <sys/stat.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/mman.h>
    #include <arpa/inet.h>
    #include <semaphore.h>
    #include <dlfcn.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <pthread.h>
    typedef uint32_t DWORD;
    #define ExitProcess exit
#endif


// misc
#ifdef _WIN32
#else
    #define Sleep(x) usleep(1000*(x))
#endif


// networking
#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define closesocket close
    #define SOCKADDR struct sockaddr
    #define GetLastError() errno
#endif


// random
#ifdef _WIN32
    #include <bcrypt.h>
    #define SECURE_RANDOM(ptr, len) BCryptGenRandom(NULL, (BYTE *)(ptr), (len), BCRYPT_USE_SYSTEM_PREFERRED_RNG)
#else
    #include <sys/random.h>
    #include <unistd.h>
    #define SECURE_RANDOM(ptr, len) (getrandom((ptr), (len), 0) < 0 ? -1 : 0)
#endif


// threading
#ifdef _WIN32
    typedef HANDLE thread_t;
    typedef SRWLOCK lock_t;
    #define INIT_LOCK SRWLOCK_INIT
    #define lock_write(x) AcquireSRWLockExclusive(x)
    #define unlock_write(x) ReleaseSRWLockExclusive(x)
    #define lock_read(x) AcquireSRWLockShared(x)
    #define unlock_read(x) ReleaseSRWLockShared(x)
    #define trylock_write(x) TryAcquireSRWLockExclusive(x)
    #define trylock_read(x) TryAcquireSRWLockShared(x)

    typedef DWORD tls_index_t;
    typedef DWORD thread_result_t;
#else
    typedef pthread_t thread_t;
    typedef pthread_rwlock_t lock_t;
    #define INIT_LOCK PTHREAD_RWLOCK_INITIALIZER
    #define lock_write(x) pthread_rwlock_wrlock(x)
    #define unlock_write(x) pthread_rwlock_unlock(x)
    #define lock_read(x) pthread_rwlock_rdlock(x)
    #define unlock_read(x) pthread_rwlock_unlock(x)
    #define trylock_write(x) (pthread_rwlock_trywrlock(x) == 0)
    #define trylock_read(x) (pthread_rwlock_tryrdlock(x) == 0)

    typedef pthread_key_t tls_index_t;
    #define TlsGetValue(x) pthread_getspecific(x)

    typedef void *thread_result_t;
#endif


#endif
