#include "inttypes.h"
#include "stdio.h"
#include "stdlib.h"

const int64_t mod = 7340033;
const int64_t root = 5;
const int64_t root_1 = 4404020;
const int64_t root_pw = 1 << 20;
 
void dft(int64_t *a, int64_t n, int64_t inv) 
{
    // pow roots
	for (int64_t i = 1, j = 0; i < n; ++i) 
	{
		int64_t bit = n >> 1;
		for (; j >= bit; bit >>= 1)
		{
			j -= bit;
		}
		j += bit;
		if (i < j)
		{
		    int64_t tmp = a[j];
		    a[j] = a[i];
		    a[i] = tmp;
		}
	}
 
	for (int64_t len = 2; len <= n; len <<= 1) 
	{
		int64_t wlen = inv ? root_1 : root;
		for (int64_t i = len; i < root_pw; i <<= 1)
		{
			wlen = wlen * wlen % mod;
		}
		for (int64_t i = 0; i < n; i += len) 
		{
			int64_t w = 1;
			for (int64_t j = 0; j < len / 2; ++j) 
			{
				int64_t u = a[i + j];
				int64_t v = a[i + j + len/2] * w % mod;
				a[i + j] = (u + v) % mod;
				a[i + j + len/2] = (u - v) % mod;
				w = w * wlen % mod;
			}
		}
	}
	if (inv) 
	{
	    int64_t nrev = 1, t = n; // n^(mod-2)
	    int64_t p = mod - 2;
	    while (p != 0)
	    {
	        if (p & 1) { nrev *= t; nrev %= mod; }
	        t *= t;
	        t %= mod;
	        p >>= 1;
	    }
		for (int64_t i = 0; i < n; ++i)
		{
			a[i] = ((a[i] * nrev) % mod + mod) % mod;
		}
	}
	else
	{
		for (int64_t i = 0; i < n; ++i)
		{
			a[i] = (a[i] % mod + mod) % mod;
		}
	}
}

int main(int argc, char **argv)
{
    int64_t t = argc-2;
    int64_t len = (t | (t >> 1) | (t >> 2) | (t >> 4) | (t >> 8) | (t >> 16) | (t >> 32)) + 1;
    int64_t *arr = malloc(8 * argc);
    for (int i = 0; i < len; ++i)
    {
        if (i + 1 < argc)
        {
            arr[i] = atoll(argv[i + 1]);
        }
        else
        {
            arr[i] = 0;
        }
    }
    // dft(arr, len, argv[1][0] == 'i');
    dft(arr, len, 0);
    for (int i = 0; i < len; ++i)
    {
        printf("%lld ", arr[i]);
    }
    printf("\n");
    return 0;
}
