
#ifndef _UPCR_UTIL_H
#define _UPCR_UTIL_H

#define UPCRI_IS_POWER_OF_TWO(n)    (!((n)&((n)-1)))

/*
 * See if we can detect power of 2 threads
 */
#if !defined(UPCRI_USING_POW2_THREADS) &&				       \
    defined(__UPC_STATIC_THREADS__) && defined(THREADS)	
    #if (THREADS == 1 || THREADS == 2 || THREADS == 4 || THREADS == 8 ||       \
     THREADS == 16 || THREADS == 32 || THREADS == 64 || THREADS == 128 ||      \
     THREADS == 256 || THREADS == 512 || THREADS == 1024 || THREADS == 2048 || \
     THREADS == 4096 || THREADS == 8192 || THREADS == 16384)
	#define UPCRI_USING_POW2_THREADS 1
    #else
	#define UPCRI_USING_POW2_THREADS 0
    #endif
#else
  #define UPCRI_USING_POW2_THREADS 0
#endif

#define UPCRI_ALIGNDOWN(p,P)   (upcri_assert(UPCRI_IS_POWER_OF_TWO(P)), \
                                ((uintptr_t)(p))&~((uintptr_t)((P)-1)))
#define UPCRI_ALIGNUP(p,P)     (UPCRI_ALIGNDOWN((uintptr_t)(p)+((uintptr_t)((P)-1)),P))
#define UPCRI_PAGEALIGNUP(p)   UPCRI_ALIGNUP(p, UPCR_PAGESIZE)
#define UPCRI_PAGEALIGNDOWN(p) UPCRI_ALIGNDOWN(p, UPCR_PAGESIZE)

/*
 * Round value down to nearest page size.
 */
GASNETT_INLINE(upcri_rounddown_pagesz)
uintptr_t
upcri_rounddown_pagesz(uintptr_t val)
{
    return val - (val % UPCR_PAGESIZE);
}

/*
 * Round value up to nearest page size.
 */
GASNETT_INLINE(upcri_roundup_pagesz)
uintptr_t
upcri_roundup_pagesz(uintptr_t val)
{
    return upcri_rounddown_pagesz(val + (UPCR_PAGESIZE -1));
}

/*
 * Divides and rounds up result.
 */
GASNETT_INLINE(upcri_ceiling_div)
uintptr_t 
upcri_ceiling_div(uintptr_t a, uintptr_t b)
{
    return (a + b - 1) / b;
}

typedef uint64_t bupc_tick_t;
GASNETT_INLINE(bupc_ticks_now)
bupc_tick_t bupc_ticks_now() { 
  return (bupc_tick_t)gasnett_ticks_now(); 
}
GASNETT_INLINE(bupc_ticks_to_us)
uint64_t bupc_ticks_to_us(bupc_tick_t ticks) {
  upcri_assert(sizeof(bupc_tick_t) >= sizeof(gasnett_tick_t));
  return (uint64_t)gasnett_ticks_to_us(ticks);
}
GASNETT_INLINE(bupc_ticks_to_ns)
uint64_t bupc_ticks_to_ns(bupc_tick_t ticks) {
  upcri_assert(sizeof(bupc_tick_t) >= sizeof(gasnett_tick_t));
  return (uint64_t)gasnett_ticks_to_ns(ticks);
}
GASNETT_INLINE(bupc_tick_granularityus)
double bupc_tick_granularityus() {
  return gasnett_tick_granularityus();
}
GASNETT_INLINE(bupc_tick_overheadus)
double bupc_tick_overheadus() {
  return gasnett_tick_overheadus();
}

extern void upcri_clock_init();
extern clock_t upcri_clock();

#endif /* _UPCR_UTIL_H */
