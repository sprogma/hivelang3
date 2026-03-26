#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define N (50*1000*1000)

int64_t count_primes() 
{
    char *sieve = malloc(N+1);

    memset(sieve, 1, N+1);
    sieve[0] = sieve[1] = 0;

    for (int64_t p = 2; p * p <= N; p++) 
    {
        if (sieve[p]) 
        {
            for (int64_t i = p * p; i <= N; i += p) 
            {
                sieve[i] = 0;
            }
        }
    }

    int64_t count = 0;
    for (int64_t i = 2; i <= N; i++) 
    {
        if (sieve[i])
        {
            count++;
        }
    }

    free(sieve);
    return count;
}


int main() 
{
    return count_primes();
}

