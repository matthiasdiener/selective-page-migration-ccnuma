#include <stdio.h>

void __pact_reuse_add(void *ary, long long start, long long end,
                      long long step, long long mult) {
  printf("__pact_reuse_add(%p, %lld, %lld, %lld, %lld)\n",
         ary, start, end, step, mult);
}

void __pact_reuse_sub(void *ary, long long start, long long end,
                      long long step, long long mult) {
  printf("__pact_reuse_sub(%p, %lld, %lld, %lld, %lld)\n",
         ary, start, end, step, mult);
}

void __pact_reuse_mul(void *ary, long long start, long long end,
                      long long step, long long mult) {
  printf("__pact_reuse_mul(%p, %lld, %lld, %lld, %lld)\n",
         ary, start, end, step, mult);
}

