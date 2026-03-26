# Gpu example

This examples shows experimental api: gpu proiver.

## Sieve 

```
@x64

[entry:yes]
[export:yes]
(i64[] input)main(i32? res)
{
    i8[] array;
    array <- new i8[](50*1000*1000);

    # 0 and 1 aren't prime
    array[0] <- array[1] <- `i8`1;
    # 2 is prime
    array[2] <- `i8`0;
    
    i8[]@gpu tmp;
    tmp <- `i8[]@gpu`array;
    ?(tmp, 3, ?array, 0)get_prime@gpu(*);
    array <- `i8[]`tmp;

    i32 id, len, ans;
    id <- 0;
    ans <- len <- `i32`?array;
    while id < len
    {
        ans <- ans - `i32`array[id];
        id <- id + 1;
    }
    res <- ans;
}

@gpu

[GPUxVariable:index]
[GPUxBase:xstart]
[GPUxSize:xend]
[integer64:yes]
(i8[] tmp, i64 xstart, i64 xend, i64 index)get_prime(i32?@x64 await)
{
    i64 val;
    val <- 3;
    while val < 64 && index % val <> 0
    {
        val <- val + 2;
    }
    match index % val = 0 || index % 2 = 0
    {-1{
        tmp[index] <- `i8`1+`i8`(index = val); # if equal -> 0 else 1
    }0{
        val <- index * index;
        while val < xend
        {
            tmp[val] <- `i8`1;
            val <- val + index;
        }
    }}
}
```

This test calculates count of primes under 5e7 using sieve on gpu. (c version is single thread cpu, to check answer).

Sieve algorithm not very good for gpu, but it don't very mean if it is example.


Compile it: in root of project
```
./a.exe --input-file=./examples/gpu/sieve.hive
```

run it: in `runtimes/0.3/win` (`n` means no input is need for program)
```
./a.exe n
```

If n 5e7, answer is 0x2DCB2E


## Life game

Bigger example, this simulates game of life, using ping pong scheme.

W and H is input of program.

[source code is in life.hive](./life.hive), there are few comments.

Compile it: in root of project
```
./a.exe --input-file=./examples/gpu/sieve.hive
```

run it: in `runtimes/0.3/win` (`n` means no input is need for program)
```
"r`n4`n100 100 1 5" | ./a.exe
```

* r - run
* 3 - number of args\
args are 
* W (100 in example)
* H (100 in example)
* draw_freq (1 in example) - count of ping pong iterations before draw (to not wase too much time on drawing and buffer passing.)
* count_of_galactics - world generation config (number of starting clusters)

(after 1 million iterations program stops) (use Ctrl+C to break it if you want)

(it is slow becouse of `pause 50` in draw loop, uncomment it to get incredible speed of gpu!)
