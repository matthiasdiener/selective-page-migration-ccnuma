all: fasan_rt.bc

fasan_rt.bc: FASanRuntime.cpp
	clang++ -O3 -emit-llvm -c FASanRuntime.cpp -o fasan_rt.bc

.PHONY: clean

clean:
	rm -f fasan_rt.bc
