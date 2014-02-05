#include <hwloc.h>
c_topology_t __pact_topo;
unsigned long __pact_cache_size;


inline void __pact_hwloc_init() {
    hwloc_topology_init(&__pact_topo);
      hwloc_topology_load(__pact_topo);

        hwloc_obj_t obj;
          __pact_cache_size = 0;

            for (obj = hwloc_get_obj_by_type(__pact_topo, HWLOC_OBJ_PU, 0); obj; obj = obj->parent) {
                  if (obj->type == HWLOC_OBJ_CACHE)
                          __pact_cache_size += obj->attr->cache.size;
                    }
}


inline void __pact_hwloc_finalize() {
    hwloc_topology_destroy(__pact_topo);
}


inline void __pact_reuse_add(void *ary, long long start, long long end, long long mem_ac) {
    hwloc_bitmap_t set = hwloc_bitmap_alloc();
    hwloc_get_cpubind(__pact_topo, set, HWLOC_CPUBIND_THREAD);
    hwloc_get_last_cpu_location(__pact_topo, set, HWLOC_CPUBIND_THREAD);
    hwloc_bitmap_singlify(set);
    hwloc_set_area_membind ( __pact_topo, (const void*)ary, abs(end-start), (hwloc_const_cpuset_t)set, HWLOC_MEMBIND_BIND, HWLOC_MEMBIND_MIGRATE );
    hwloc_bitmap_free(set);
}

hwloc_topology_t __pact_topo;
unsigned long __pact_cache_size;


inline void __pact_hwloc_init() {
	hwloc_topology_init(&__pact_topo);
	hwloc_topology_load(__pact_topo);
    
	hwloc_obj_t obj;
	__pact_cache_size = 0;
	
	for (obj = hwloc_get_obj_by_type(__pact_topo, HWLOC_OBJ_PU, 0); obj; obj = obj->parent) {
		if (obj->type == HWLOC_OBJ_CACHE)
			__pact_cache_size += obj->attr->cache.size;
	}
}


inline void __pact_hwloc_finalize() {
	hwloc_topology_destroy(__pact_topo);
}


inline void __pact_reuse_add(void *ary, long long start, long long end, long long mem_ac) {
	
	//put heuristic here!
	
	hwloc_bitmap_t set = hwloc_bitmap_alloc();
		
	hwloc_get_cpubind(__pact_topo, set, HWLOC_CPUBIND_THREAD);
	hwloc_get_last_cpu_location(__pact_topo, set, HWLOC_CPUBIND_THREAD);
	
	hwloc_bitmap_singlify(set);
	hwloc_set_area_membind ( __pact_topo, (const void*)ary, abs(end-start), (hwloc_const_cpuset_t)set, HWLOC_MEMBIND_BIND, HWLOC_MEMBIND_MIGRATE );

	hwloc_bitmap_free(set);
}

/*
	void __pact_reuse_sub(void *ary, long long start, long long end,long long trip, long long mult) {
		printf("__pact_reuse_sub(%p, %lld, %lld, %lld, %lld)\n", ary, start, end, trip, mult);
	}
*/


//functions below can be removed? 

/*
void __pact_minmax_add(long long f, long long s, long long* cmin, long long* cmax) {
  long long lmin = *cmin, lmax = *cmax;
  if (f < s) {
    *cmin += f;
    *cmax += s;
  } else {
    *cmin += s;
    *cmax += f;
  }
  // printf("__pact_minmax_add(%lld, %lld, %lld => %lld, %lld => %lld)\n", f, s, lmin, *cmin, lmax, *cmax); 
}

void __pact_minmax_mul(long long f, long long s, long long* cmin, long long* cmax) {
  long long lmin = *cmin, lmax = *cmax;
  long long a = lmin * f, b = lmin * s, c = lmax * f, d = lmax * s;
  *cmin = min(a, min(b, min(c, d)));
  *cmax = max(a, max(b, max(c, d)));
  // printf("__pact_minmax_mul(%lld, %lld, %lld => %lld, %lld => %lld)\n", f, s, lmin, *cmin, lmax, *cmax); 
}
*/
