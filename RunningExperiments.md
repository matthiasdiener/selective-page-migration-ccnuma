# Benchguide: how to run benchmarks in our machine #

This guide aims to organize the information concerning the execution of experiments using benchmarks in our server. Our goal is to have an automatic framework to help the automatization proccess of adding and executing benchmarks using our infrastructure - ideally using Python scripts.


## Methods of page migration/memory allocation (or what can i do to try improving speed of applications?) ##


First thing is to understand a little about how computer memory can be organized. In 'old' desktop systems, all memory was always structured together, in an uniform manner, and the whole memory space was accesible by the programmer. Nowadays, it's still true that a programmer can access all the memory, but memory is now structured in a non-uniform manner, specially because of the popularity of multicore processors. These 'scattered' arrangements of memory compose systems called cache-coherent Non-Uniform Memory Access systems ([ccNUMA systems](http://en.wikipedia.org/wiki/Non-uniform_memory_access)).


These systems aims to reduce contention problems caused by having multiple cores trying to access data in memory via only one BUS. So, scattering memory in different controllers is supposed a solution for this problem - these separated controllers are connected through an interconnection network, allowing the programmer to use/access all the memory. But ccNUMA systems still can have contention problems if the data is mis-allocated by the programmer.

Modern operating systems use a lazy-allocation policy, so the memory is only really allocated when the data is touched - if a multithreaded program touches the data in its main thread, all data is allocated on the NUMA node running the main thread, and the other threads data accesses will cause contention. Even if the programmer take care of allocating/touching memory inside threads, ccNUMA systems can have contention because threads are scheduled multiple times among cores. One final problem of ccNUMA system is that the remote access (means accessing non-local data, in a remote memory controller) 'costs' more than local access, so it's important to group threads and data locally.

That said, we want to improve data distribution among cores by dynamically migrating data. There are many methods to improve memory allocation, listed below:


### `numactl` ###
[numactl](http://oss.sgi.com/projects/libnuma/) is a Linux tool that allows one to invoke any application with a specific memory allocation policy. In other words, one can run a process specifying that the memory allocation will be done in  a round-robin fashion, apart from the programmer's choice in the code. This tool is able to eliminate memory contention due to mis-allocated memory by the lazy policy, but still remains the problem of the high costs of remote memory accesses.

To use `numactl` you must invoke your application as a parameter of the tool, like this:

`numactl --interleave all ./your_program [your_arguments]`

There are a bunch of parameters of `numactl`; the `interleave -all` specify that memory must be allocated scattered among the memory controllers. One can e.g. use the parameter `--cpunodebind N` to force the process to run only in the CPUs attached to node (a.k.a. memory controller) `N`.

The downsides of `numactl` are that it doesn't solve the remote access high costs problem, and it may interfere with a programmer's tunning on the source code for better memory allocation. Besides this, it has not too much performance gains - they are not scalable.

### `mbind(...)`/`move_pages(...)` call ###

`mbind` and `move_pages` are wrappers to Linux syscalls that allow the programmer to migrate pages dynamically to the chosen node(s). Both functions work in a similar manner, but `mbind` can migrate a data structure while `move_pages` migrates pages - the programmer must fill an array with the addresses of memory pages to be migrated in the latter.

It seems `mbind` is a bit faster than `move_pages`, probably because once `mbind` is used, there's no need to calculate the memory pages of a data structure - it can be moved directly. The declaration of `mbind` is

`int mbind(void *addr, unsigned long len, int mode, unsigned long *nodemask, unsigned long maxnode, unsigned flags);`

It was originally a memory policy selection function, but one can use the flag `MPOL_MF_MOVE` to make the function be able to migrate memory pages. The syntax of `move_pages` is

`long move_pages(int pid, unsigned long count, void **pages, const int *nodes, int *status, int flags);`

Both functions must be linked with `libnuma`, and both are expensive - so one must take care in using them.

### Minas: Memory Affinity Management Framework ###

[Minas](http://hal.inria.fr/inria-00421546) is a framework that aims to improve memory allocation, either via manual function insertion or automatic code analysis and function replacement. Minas provides its own version of allocation routines like `malloc` and `free`. The programmer can change the source code in order to benefit from this functions, or a Minas tool can be used to do an automatic analysis of the source code and modify the promising `malloc/free` calls (e.g., shared arrays allocation calls in OpenMP programs).

Our use is limited to manual change all the calls directly in source code. Minas has low overhead, but has low performance gains too.

### SPM: Selective Page Migration (a.k.a. our optimization idea) ###

It seems we have 2 options to improve performance of applications running in ccNUMA systems: change the source code to make use of better allocation functions and/or page migration routines or invoke some process with modified memory allocation policies, using tools like `numactl` to achieve this. Actually, another approach exists, but it wasn't discussed here: resort the operating system to improve memory allocation, in an application-transparent fashion. The last approach seems very interesting, but has a main - and serious - downside: it demands kernel modifications, which is not so easy as it seems :-)

Besides, both OS and external tools (e.g. `numactl`) approaches have one big complexity in implementation: how to deal with general applications allowing performance gains in all kinds of programs, regardless the code structure or manual tunning in source code done by the programmer? In other words: how to be general? It's really not easy to answer this in a satisfying manner...

So, we propose here a compiler approach, called Selective Page Migration (SPM). In short, we do a static analysis at compile time to infer the size and reuse of data structures, using a loop trip count predictor. Then, we insert code to be filled dynamically with values that only will be available at runtime. Finally, comparisons against some thresholds can be done on program execution to determine if it worths to migrate the pages of some data structures. Our experimental results gave us good performance improvements, but still inferior than manual approaches. We plan to improve our static analysis and heuristics to obtain better results...

We made use of the [LLVM Compiler Infrastructure](http://llvm.org) to implement our idea - to call our compiler analysis/transformation, one must first compile the library and attach it in the LLVM infrastructure and then compile the desired source code applying the optimization. More details of the process are in the next subsection.


#### How to use SPM? ####

Using SPM requires LLVM 3.3 (built with RTTI) and the following dependencies:
  * [SymPy](http://sympy.org) (latest version) - don't forget to add the `lib.*/` directory from the SymPy build to your `PYTHONPATH`.
  * [GiNaC 1.6.2](http://www.ginac.de) - add GiNaC's `<build>/lib/` directory to your `LD_LIBRARY_PATH`.
  * [hwloc 1.8](http://www.open-mpi.org/projects/hwloc) - again, add hwloc's `<build>/lib/` directory to your `LD_LIBRARY_PATH`.

Once it's done, you can get SPM from this repo. The LLVM optimization library is in  `${repo_clone_path}/src/SelectivePageMigration` - you can use the Makefile to build the Shared Object (`.so` file - we are talking about Linux here!).

Now, you must compile SPM's runtime component, which is the file  `${repo_clone_path}/src/SelectivePageMigration/Runtime/SelectivePageMigrationRuntime.cpp`. The generated object (`.o` file) must be linked together with the programs compiled with SPM. Note that this file requires the compilation flag `-std=c++0x`, since it makes use of C++11.

Finally, having completed all the steps above, the infrastructure is ready to be used. To compile a program using SPM, one must follow the next 4 steps:

  1. Compile some C/C++ source code with `-O0 -emit-llvm` flags, generating bitcode files (`.bc`) instead of objects.
  1. Apply the following sequence of optimizations, using LLVM's `opt` tool: `-mem2reg -SPM -O3`. Don't forget to load the SPM's library here!
  1. Generate the object for the specified architecture, using the LLVM's `llc` tool - note this will generate an assembly file, which is not pure object-binary file, but 'means' the same, and can be used interchanged with object files.
  1. Link all objects generated together with the SPM's runtime component and hwloc library (`-lhwloc`).

An example of the above sequence of steps, for an application called `Test-SPM` which use Pthreads and has one single source file called `Test-SPM.c` is:

`clang -O0 -emit-llvm -c -o T.bc Test_SPM.c`

`opt -mem2reg -load ${path_to_so_library_file}/SelectivePageMigration.so -spm -O3 T.bc -o T.opt.bc`

`llc T.opt.bc -o Test-SPM.s`

`clang -O3 Test-SPM.s ${path_to_SPM's_runtime_module}/SPM.o -o Test-SPM -lpthread -lhwloc`

**Note:** except when the opposite is explicitly declared, it's recommended to use -O3 flag always, in any of the steps above.


## Benchmarks (or where can i test my ideas in a standardize way?) ##

Besides standard benchmark packages, like [Parsec 3.0](http://parsec.cs.princeton.edu), we've made experiments using our own set of benchmarks, called "Easy" set. Basically, we have 2 types of Easy benchmarks: one that makes use of a class called `arglib` to evaluate the passed arguments, and the other that don't use this class, parsing the arguments in a manual/case-by-case fashion. We will describe briefly the benchmarks below, splitting by type.

### `arglib` benchmarks ###

**Note:** The path of benchmarks is currently `/local/ufmg/thiago`, on our server, which is henceforth mentioned as `${path`}.

The `arglib` is a very interesting class that allow us to easily deal with argument evaluation/parsing in C++ programs. It works by adding the header `#include "arglib.h"` in the beginning of source file, and than
one must global-instantiate the class creating one object by desired argument. For example, if one wants an argument `-sz X`, where X means something's size (like matrix dimension), it's only needed to add

`clarg::argInt sz("-sz", "Matrix dimension, or whatever you want as description!", 100);`

to the source code, in the global section. The arglib object (`clarg` is the name of class) is called `sz`, the expected argument is `"-sz"`, default value is 100, type is integer (`argInt` !) and the description is self-explanatory. Another types can be `argBool`, `argDouble` and `argString`. Once it's done, it's mandatory to put the following line in the beginning of the `main` function:

`clarg::parse_arguments(argc, argv);`

Then one can access the values passed by arguments using `get_value()` member function or check whenever an argument is passed using `was_set()`.

That said, let's focus now on the `arglib`-benchmarks:

  * **easy\_lu:** Default LU factorization;
  * **easy\_ch:** Default Cholesky decomposition;
  * **easy\_prod:** Naive matrix multiplication;
  * **easy\_add:** Default matrix addition;

All these 4 benchmarks share many characteristics: the matrices are double-typed and have their dimension passed by `-msz` argument; it's mandatory to pass the arguments `-nt` and `-nm`, which are respectively the number of threads and the number of matrices each thread will process. All these benchmarks make use of the Pthreads library and allocate all their matrices in the main thread. The output is only one double-typed number, the time the threads run (measured from `pthread_create` until `pthread_join` calls).

Although these benchmarks are considered `arglib` ones, they use preprocessor conditional definitions (`#ifdefs`) to enable/disable the migration functions, so if one wants to test these benchmarks always migrating pages (no heuristic/SPM), it must be passed `-D__MIGRATE__` compilation flag.

The Minas versions of these benchmarks are in `${path}/minas` and also make use of `#ifdefs` - one must pass the parameter `-D__MAI__` on compilation to test the benchmarks with Minas.


### non-`arglib` ones ###

Non-`arglib` benchmarks are 5, and they all use the  same `ifdefs` above explained. The argument passing mechanism is done manually, each benchmark has its own order and quantity of arguments, as explained below:

  * **BucketSort:** Default bucket sort (a.k.a. bin sort) implementation, the arguments to be passed are: `NumElements NumThreads [seed]`;
  * **kMeans:** Default k-means clustering algorithm, the arguments to be passed are: `NumElements NumClusters NumThreads [seed]`;
  * **KNN:** Standard k-Nearest Neighbors algorithm implementation, the arguments to be passed are: `num_threads num_points K`;
  * **QR:** Default QR matrix decomposition implementation, the arguments to be passed are: `num_threads num_matrices matrix_dimension`;
  * **partitionStringSearch:** Dummy string search algorithm implementation. Initially populate an array with some characters in the range 'a-z', then give to each thread the tasks of: generate `num_queries` patterns, count the number of occurrences of the pattern in the initial array and report the maximum number of occurrences. The arguments to be passed are: `num_threads str_size num_queries`, but this benchmark has some important `#defines` that can be modified, like `MINIMUM_STRING_SIZE` and `MINIMUM_STRING_SIZE`.

The Minas versions of these benchmarks are in `${path}/minas` and also make use of `#ifdefs` - one must pass the parameter `-D__MAI__` on compilation to test the benchmarks with Minas.

## References ##

Christoph Lameter - [Local and remote memory: Memory in a Linux/NUMA system](http://ftp.be.debian.org/pub/linux/kernel/people/christoph/pmig/numamemory.pdf)


Ulrich Drepper - [What Every Programmer Should Know About Memory](http://people.freebsd.org/~lstewart/articles/cpumemory.pdf)


Christiane Pousa Ribeiro, Jean-François Méhaut - [Minas: Memory Affinity Management Framework](http://hal.inria.fr/inria-00421546)


## TODO List (or how can i improve the organization of the benchmarks?) ##

_Make use of `arglib` in all benchmarks (eliminating most `#ifdefs`) and determine a standard about the arguments passed to our benchmarks_

_Create Python (or-any-language) scripts to facilitate the execution of the benchmarks and the output of the results_

_Standardize the output results - which time must be showed?_

_Improve memory checks on our benchmarks, like `malloc` returning NULL, etc_

_'Improve' formal statistics behind the experimental results (see: http://reproducibility.cs.arizona.edu)_

_Add Parsec and probably one or two more 'big' benchmark suites in our test framework_

_Improve this wiki_

_Add more things in this TODO section :-)_