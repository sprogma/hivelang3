# Examples

If you're using the wrong os (linuks (~~gnu😮~~)), please check its [directory](./linux).

## Examples

(order isn't random)

* ### [helloworld](./helloworld/)
    Start from here, see how language is built, and how to make something working.

* ### [constructions](./constructions/)
    Many examples that shows constructions supported in hive language.

* ### [dllimport](./dllimport/)
    Example of usage of dll provider, to show messagebox.
    Dll provider helps you to interact with native code.

* ### [dft](./dft/)
    Speed comparison against C in [dft](https://en.wikipedia.org/wiki/Discrete_Fourier_transform).
    This is single thread task, where measured speed of accessing arrays and integer calculations.

* ### [qsort](./qsort/) Speed comparison agains C in qsort.
    Speed comparison against C in sorting.
    Here, hive program is written in attempt to use parallelism of quicksort algorithm, it shows
    how to write high-performance calculations with hivelang

* ### [gpu](./gpu/)
    Example of usage of gpu provider. Is allows you to run
    almost common hive functions on gpu. Directory contains two gpgpu examples.

* ### [game](./game1/) [in progress]
    This is simple multiplayer game, based on winapi, which show how easy use networking in
    hivelang. -- For now, it works mainly on one server, network is broken in 0.3 runtime.