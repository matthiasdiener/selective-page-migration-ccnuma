-- Prerequisites --
1) Download and install the latest version of SymPy from
   git://github.com/sympy/sympy.git (requires Python 2.7).
   Add the lib.*/ directory from the SymPy build to your PYTHONPATH.
2) Download and install GiNaC 1.6.2 from
   ftp://ftpthep.physik.uni-mainz.de/pub/GiNaC/ginac-1.6.2.tar.bz2 and apply the
   patch ginac.diff from the SPM repository. Add GiNaC's <build>/lib/ directory
   to your LD_LIBRARY_PATH.
3) Download and install hwloc from
   http://www.open-mpi.org/software/hwloc/v1.8/downloads/hwloc-1.8.1.tar.gz.
   Add hwloc's <build>/lib/ directory to your LD_LIBRARY_PATH.

-- Compiling --
1) Add GiNaC's <build>/include/ directory to CXXFLAGS preceded by -I before
   running make, if necessary.
2) Run make.

-- Using --
1) Compile the input file to bytecode with -O0.
2) Run "opt -mem2reg -load SelectivePageMigration.so -spm in.ll -o out.ll".
   You may specify a single function to be transformed with
   -spm-pthread-function.
3) Generate an object file from out.ll with llc & gcc/clang. You may choose
   to optimize when running llc.
4) Compile the runtime with
   "g++ -O3 -std=c++0x SelectivePageMigrationRuntime.cpp -c".
5) Link the object file with SelectivePageMigrationRuntime.o and with
   hwloc using -lhwloc.

