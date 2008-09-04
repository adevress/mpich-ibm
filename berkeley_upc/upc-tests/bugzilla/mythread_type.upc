#include <upc.h>
#include <stdio.h>

/* This bug never made it as far as Bugzilla.
 * The UPC Spec v1.1.1 says explicitly that MYTHREAD has type 'int'.
 * We used to get this wrong.
 */
int main(void)
{
  int failed = 0;

#ifdef __BERKELEY_UPC__
  /* Bonus compile-time check */
  bupc_assert_type(MYTHREAD, int);
#endif

#if 0	/* actual behavior is undefined?? broken test. */
  /* Check signed - if unsigned this will fail */
  if (MYTHREAD < -1) {
	printf("FAILED: MYTHREAD is unsigned\n");
	failed = 1;
  }
#endif

  /* Check size */
  if (sizeof(MYTHREAD) != sizeof(int)) {
	printf("FAILED: MYTHREAD has wrong size\n");
	failed = 1;
  }

  if (!failed)
	printf("SUCCESS\n");

  return !failed;
}
