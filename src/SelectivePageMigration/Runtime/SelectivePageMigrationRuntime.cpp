#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <hwloc.h>


#ifdef __DEBUG__
#define SPMR_DEBUG(X) X
#else
#define SPMR_DEBUG(X)
#endif

extern "C" {
  void __spm_init();
  void __spm_end();
  void __spm_get (void *Array, long Start, long End, long Reuse);
  //void __spm_give(void *Array, long Start, long End, long Reuse);
}

const double __spm_ReuseConstant = 200.0;
const double __spm_CacheConstant = 0.1;

const long PAGE_EXP  = 12;
const long PAGE_SIZE = (1 << PAGE_EXP);

hwloc_topology_t __spm_topo;
unsigned long __spm_cache_size = 0;

class PageIntervals {
private:
  struct PageRegion {
    long Start, End;
    bool operator<(const PageRegion& Other) const {
		SPMR_DEBUG(std::cout << "Runtime: (" << Start << ", " << End << ") < ("
							 << Other.Start << ", " << Other.End << ") == "
							 << (Start < Other.Start ? End < Other.End : false) << "\n");
      return Start < Other.Start ? End < Other.End : false;
    }
  };

public:
  typedef std::map<PageRegion, pid_t> RegionsTy;

  RegionsTy::iterator insert(long Start, long End, pid_t Idx);
  RegionsTy::iterator begin() { return Regions_.begin(); }
  RegionsTy::iterator end()   { return Regions_.end();   }

private:
  RegionsTy::iterator join(RegionsTy::iterator L);
  RegionsTy::iterator join(RegionsTy::iterator L, RegionsTy::iterator R);

  RegionsTy Regions_;
};

PageIntervals::RegionsTy::iterator PageIntervals::insert(long Start, long End,
                                                         pid_t Idx) {
  SPMR_DEBUG(std::cout << "Runtime: inserting region (" << Start << ", "
                       << End << ")\n");
  PageRegion R = { Start, End };
  auto It = Regions_.insert(std::make_pair(R, Idx));
  if (It.second) {
    // Region was inserted - attempt to join it with its neighbors.
    auto RIt = join(It.first);
    SPMR_DEBUG(std::cout << "Runtime: iterator for region (" << Start << ", "
                         << End << ") is (" << RIt->first.Start << ", "
                         << RIt->first.End << ")\n");
    return RIt;
  } else {
    // Not inserted - an iterator already exists for this region.
    SPMR_DEBUG(std::cout << "Runtime: iterator already exists for region ("
                         << Start << ", " << End << ")\n");
    return end();
  }
}

PageIntervals::RegionsTy::iterator PageIntervals::join(RegionsTy::iterator L) {
  RegionsTy::iterator It = L; ++It;
  // Join with the greater (>) subtree.
  if (It != Regions_.end()) {
    L = join(L, It);
  }
  // Join with the lesser (<) subtree.
  if (L != Regions_.begin()) {
    RegionsTy::iterator It = L; --It;
    L = join(L, It);
  }
  return L;
}

PageIntervals::RegionsTy::iterator PageIntervals::join(RegionsTy::iterator L,
                                                      RegionsTy::iterator R) {
  // TODO: try to lease a single conflicting page to multiple threads - it
  // could be an array boundary.
  if (L->second != R->second) {
    SPMR_DEBUG(std::cout << "Runtime: region conflicts with other thread: ("
                         << L->first.Start << ", " << L->first.End << ") and ("
                         << R->first.Start << ", " << R->first.End << ")\n");
    return L;
  }
  /*
   * L |---- ---- ---- ----|
   * R           |---- ---- ---- ----|
   */
  if (L->first.Start <= R->first.Start && L->first.End <= R->first.End) {
    long Start = L->first.Start, End = R->first.End, Idx = L->second;
    Regions_.erase(L);
    Regions_.erase(R);
    return insert(Start, End, Idx);
  }
  /*
   * L           |---- ---- ---- ----|
   * R |---- ---- ---- ----|
   */
  if (R->first.Start <= L->first.Start && R->first.End <= L->first.End) {
    long Start = R->first.Start, End = L->first.End, Idx = L->second;
    Regions_.erase(R);
    Regions_.erase(L);
    return insert(Start, End, Idx);
  }
  /*
   * L      |---- ----|
   * R |---- ---- ---- ----|
   */
  if (L->first.Start >= R->first.Start && L->first.End <= R->first.End) {
    Regions_.erase(L);
    return R;
  }
  /*
   * L |---- ---- ---- ----|
   * R      |---- ----|
   */
  if (R->first.Start >= L->first.Start && R->first.End <= L->first.End) {
    Regions_.erase(R);
    return L;
  }
  /*
   * L |---- ---- ---- ----|
   * R                         |---- ---- ---- ----|
   */
  assert(L->first.End < R->first.Start || L->first.Start > R->first.End ||
         L->second != R->second);
  return L;
}

static PageIntervals SPMPI;
static std::mutex    SPMPILock;


void migrate(long PageStart, long PageEnd) {
  SPMR_DEBUG(std::cout << "Runtime: migrate pages: " << PageStart << " to "
                       << PageEnd << "\n");
  SPMR_DEBUG(std::cout << "Runtime: hwloc call: " << (PageStart << PAGE_EXP)
                       << ", " << ((PageEnd - PageStart) << PAGE_EXP) << "\n");

  hwloc_bitmap_t set = hwloc_bitmap_alloc();

  hwloc_get_cpubind(__spm_topo, set, HWLOC_CPUBIND_THREAD);
  hwloc_get_last_cpu_location(__spm_topo, set, HWLOC_CPUBIND_THREAD);

  hwloc_bitmap_singlify(set);

	assert(
			hwloc_set_area_membind(__spm_topo, (const void*)(PageStart << PAGE_EXP),
								  (PageEnd - PageStart) << PAGE_EXP,
								  (hwloc_const_cpuset_t)set, HWLOC_MEMBIND_BIND,
								  HWLOC_MEMBIND_MIGRATE)
	!= -1 && "Unable to migrate requested pages");
                         
  hwloc_bitmap_free(set);
}

void __spm_init() {
  SPMR_DEBUG(std::cout << "Runtime: initialize\n");
  hwloc_topology_init(&__spm_topo);
  hwloc_topology_load(__spm_topo);

  hwloc_obj_t obj;
  for (obj = hwloc_get_obj_by_type(__spm_topo, HWLOC_OBJ_PU, 0); obj;
       obj = obj->parent)
    if (obj->type == HWLOC_OBJ_CACHE)
      __spm_cache_size += obj->attr->cache.size;

}

void __spm_end() {
  SPMR_DEBUG(std::cout << "Runtime: end\n");
  hwloc_topology_destroy(__spm_topo);
}


void __spm_get(void *Ary, long Start, long End, long Reuse) {
	SPMR_DEBUG(std::cout << "Runtime: get page for: " << (long unsigned)Ary
		<< ", " << Start << ", " << End << ", "
		<< Reuse << "\n");

	long PageStart = ((long)Ary + Start)/PAGE_SIZE;
	long PageEnd   = ((long)Ary + End)/PAGE_SIZE;


	if ( (double)(End-Start) > __spm_CacheConstant*__spm_cache_size && (double)Reuse/(End-Start > 0 ? End-Start : 100000) > __spm_ReuseConstant ) { //heuristic

/*
		SPMR_DEBUG(std::cout << "Runtime: get pages: " << PageStart << ", "
			<< PageEnd << "\n");

		pid_t Idx = syscall(SYS_gettid);

		SPMPILock.lock();
		auto It = SPMPI.insert(PageStart, PageEnd, Idx);
		
		if (It == SPMPI.end()) {
			SPMPILock.unlock();
			return;
		}
		SPMPILock.unlock();

		SPMR_DEBUG(std::cout << "Runtime: thread #" << Idx
			<< " now holds pages (possibly amongst others): "
			<< It->first.Start << " to " << It->first.End << "\n");
*/
		return (void) migrate(PageStart, PageEnd);

	}//heuristic
}

//void __spm_give(void *Array, long Start, long End, long Reuse) {
  // Currently unused.
//}

