/*
 * Broadcast function for collective allocations and a few other startup tasks.
 */

#include <upcr_internal.h>

/* Broadcasts data from one UPC thread to all others.
 *
 * Does not guarantee that all threads have received the data when any one of
 * them returns from this function--use a barrier after the function if you
 * need that (like UPC's MY/MY or MPI).
 *
 */
void 
_upcri_broadcast(upcr_thread_t from_thread, void *addr, size_t len UPCRI_PT_ARG)
{
    UPCRI_PASS_GAS();

    gasnet_coll_broadcast(GASNET_TEAM_ALL, addr, from_thread, addr, len,
                          GASNET_COLL_LOCAL | GASNET_COLL_IN_MYSYNC | GASNET_COLL_OUT_MYSYNC);
}
