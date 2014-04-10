#if 0
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
// #include <mutex>
// #include <unordered_map>
// #include <unordered_set>
#endif

#ifdef __DEBUG__
#define SPMR_DEBUG(X) X
#else
#define SPMR_DEBUG(X)
#endif

extern "C" {
  // void __spm_init();
  // void __spm_end();
  bool __fasan_check (void *Array, long Start, long End, long Reuse);
  
  // will be treated like a memory access by AddressSanitizer
  void __fasan_touch (char*Ptr);
  
  // will return true, if the address at Ptr is unpoisoned
  bool __fasan_verify (char*Ptr);
  
  //void __spm_give(void *Array, long Start, long End, long Reuse);
}

const double __spm_ReuseConstant = 200.0;
const double __spm_CacheConstant = 0.1;

#if 0
const long PAGE_EXP  = 12;
const long PAGE_SIZE = (1 << PAGE_EXP);

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
// static std::mutex    SPMPILock;
#endif

// FIXME Does "End" really point to the last byte used in the array access
bool __fasan_check(void* Ary, long Start, long End, long Reuse)
{
    char * rawAry = reinterpret_cast<char*>(Ary);
    for (long i = Start;i < End; i+= 4) {
#if 0
        __fasan_touch(rawAry + i); // TODO replace by shadow map query
#else
        if (__fasan_verify(rawAry + i)) {
            return false;
        }
#endif
    }    
    return true;
    
#if 0    
    return End > Start;

    SPMR_DEBUG(std::cout << "Runtime: get page for: " << (long unsigned)Ary
		<< ", " << Start << ", " << End << ", "
		<< Reuse << "\n");
    
    //heuristic
    if ( (double)Reuse/(End-Start > 0 ? End-Start : 100000) > __spm_ReuseConstant ) { 
        return true;
    } else {
        return false;
    }
#endif
}

