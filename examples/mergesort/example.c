#include "inttypes.h"
#include "stdio.h"
#include "stdlib.h"


void merge(int64_t *res, int64_t *a, int64_t *b, int64_t a_len, int64_t b_len)
{
    int64_t *a_end = a + a_len;
    int64_t *b_end = b + b_len;
    while (a < a_end && b < b_end)
    {
        if (*a < *b)
        {
            *res++ = *a++;
        }
        else
        {
            *res++ = *b++;
        }
    }
    while (a < a_end)
    {
        *res++ = *a++;
    }
    while (b < b_end)
    {
        *res++ = *b++;
    }
}



int64_t *sort(int64_t *arr, int64_t len)
{
    if (len == 0) return NULL;
    if (len == 1)
    {
        int64_t *tmp = malloc(8);
        *tmp = *arr;
        return tmp;
    }
    int64_t m = len / 2;
    int64_t *a = sort(arr, m);
    int64_t *b = sort(arr + m, len - m);
    int64_t *res = malloc(sizeof(*res) * len);
    merge(res, a, b, m, len - m);
    return res;
}
 

int main()
{
    int64_t len;
    scanf("%lld", &len);
    int64_t *arr = malloc(8 * len);
    for (int i = 0; i < len; ++i)
    {
        scanf("%lld", &arr[i]);
    }

    arr = sort(arr, len);

    int64_t res = 0;
    for (int i = 0; i < len; ++i)
    {
        // printf("%lld ", arr[i]);
        res += arr[i] * i;
    }
    // printf("\n");
    
    return res;
}
