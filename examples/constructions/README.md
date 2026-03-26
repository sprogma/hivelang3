# Constructions

## Many examples
[example.hive](./example.hive)
```hive
@x64

# --- function - it calculates a + b
(i32 a, i32 b)Add(i32? res)
{
    res <- a + b; # also may be - * / %
}

# --- match
(i32 num)Select(i32? res)
{
    match num
    {
        0 {
            res <- 1;
        }
        1 {
            res <- 10;
        }
        2 {
            res <- 100;
        }
        # default case is in plans...
    }
}

# --- if else:
(i32 num)IsPositive(i32? res)
{
    match num < 0 # compare operators return -1 if true, else 0.
    {
        -1 { # if true
            res <- 1;
        } 0 { # else
            res <- 0;
        }
    }
}

# --- arrays
(i32 n)fib(i32? res)
{
    i32[] numbers;
    numbers <- new i32[](n + 2); # create new array of n+2 elements
    numbers[0] <- 0;
    numbers[1] <- 1;
    i32 id;
    id <- 2;
    while id <= n
    {
        numbers[id] <- numbers[id - 1] + numbers[id - 2];
        id <- id + 1;
    }
    res <- numbers[n];
    # free operator is coming...
}

# --- structures vs classes
struct point1
{
    i32 x, y;
}
class point2
{
    i32 x, y;
}

# does nothing becouse point1 is given by copy
(point1 p)Update1(i32? res)
{
    p.x <- p.x + 1;
    res <- 0;
}

# update p, becouse point2 is given by pointer
(point2 p)Update2(i32? res)
{
    # instead of . in classes used ? - this means that
    # this is awaiting point (can hang for example
    # if class is on another machine)
    p?x <- p?x + 1;
    res <- 0;
}

()Test(i32? res)
{
    i32 sum;
    sum <- 0;
    point1 p1;
    point2 p2;

    # structure is created automatically
    p1.x <- 1; # simply set fields
    p1.y <- 1;
    ?(p1)Update1(*);
    sum <- sum + p1.x;

    # class is need to be allocated
    p2 <- new point2();
    p2?x <- 1; # simply set fields using ? instead of . (field init from constructor is unsupported for now)
    p2?y <- 1;
    ?(p2)Update2(*);
    sum <- sum + p2?x;

    res <- sum;
}

# --- built-in parallelism (async)
(i32 x)HeavyWork(i32? res)
{
    sleep 1;
    res <- x;
}

(i32 num)RunWork(i32? res)
{
    i32?[] tasks;
    i32 sum;
    tasks <- new i32?[](num);
    while num > 0
    {
        num <- num - 1;
        tasks[num] <- (num)HeavyWork(*); # start function, and save it's task to array
    }
    num = `i32`?tasks;
    sum <- 0;
    while num > 0 # wait for all items in array
    {
        sum <- sum + ?tasks[num]; # await
    }
    res <- sum; # return sum of return codes
}

## --- runs examples
[export:yes]
[entry:yes]
(i64[] input)main(i32? res) 
{
    # res <- ?(1, 2)Add(*); # -> 3  (1 + 2)
    # res <- ?(1)Select(*); # -> 10
    # res <- ?(-15)IsPositive(*); # -> 0
    # res <- ?(6)fib(*); # -> 8
    # res <- ?()Test(*); # -> 3
    # res <- ?(5)RunWork(*); # -> 10  (0 + 1 + ... + 4), executes in 1 second.
}
```

This program shows diffrent parts of language.
To run test - uncomment it inside main function.

Build program: in root of project
```
./a.exe --input-file=./examples/constructions/example.hive
```

run it: in `runtimes/0.3/win` (`n` means no input is need for program)
```
./a.exe n
```

you will see big log, which will end on something `Program exited with code XXXXXXXXXXXXXXXX`.
- this is return code of main. (in hex format)
