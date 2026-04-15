#include "windows.h"
#include "inttypes.h"
#include "stdio.h"
#include "stdlib.h"

void insertion_sort(int64_t *arr, size_t n)
{
    size_t j = 0;
    for (size_t i = 0; i < n; ++i)
    {
        int64_t pivot = arr[i];
        for (j = i; j > 0 && arr[j - 1] > pivot; --j)
        {
            arr[j] = arr[j - 1];
        }
        arr[j] = pivot;
    }
}

static inline void swap(int64_t *a, int64_t *b)
{
    int64_t t = *b;
    *b = *a;
    *a = t;
}

void split(int64_t *arr, size_t pivot_index, size_t n, size_t *left, size_t *right)
{
    int64_t pivot = arr[pivot_index];
    swap(arr + pivot_index, arr + (n - 1));
    
    int64_t i = 0;
    int64_t j = n - 2;

    while (i <= j) 
    {
        while (i + 1 < (int64_t)n && arr[i] < pivot) 
        {
            i++;
        }
        while (j >= 0 && arr[j] > pivot)
        {
            j--;
        }
        if (i >= j) break;
        swap(arr + (i++), arr + (j--));
    }
    
    swap(arr + i, arr + (n - 1));

    *left = i;
    *right = i + 1;
}

void quick_sort(int64_t *arr, size_t n) 
{
    while (n > 128)
    {
        size_t pivot_index = n / 2;
        size_t lt, gt;
        split(arr, pivot_index, n, &lt, &gt);

        if (lt < n - gt) 
        {
            quick_sort(arr, lt);
            arr += gt;
            n -= gt;
        }
        else
        {
            quick_sort(arr + gt, n - gt);
            n = lt;
        }
    }
    insertion_sort(arr, n);
}


int main()
{
    int64_t len;
    scanf("%c", (char *)&len); // skip 'r' command
    
    scanf("%lld", &len);
    int64_t *arr = malloc(8 * len);
    for (int i = 0; i < len; ++i)
    {
        scanf("%lld", &arr[i]);
    }


    LARGE_INTEGER frequency;
    LARGE_INTEGER start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    quick_sort(arr, len);

    QueryPerformanceCounter(&end);
    printf("Program finished in %lld ms\n", (end.QuadPart - start.QuadPart) * 1000 / frequency.QuadPart);

    int64_t res = 0;
    for (int i = 0; i < len; ++i)
    {
        // printf("%lld ", arr[i]);
        res += arr[i] * i;
    }
    // printf("\n");

    free(arr);
    printf("exit code: %08X\n", (int32_t)res);
    return res;
}
