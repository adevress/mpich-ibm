/* A simple test of bupc_atomic extensions */

#include <upc.h>
#include <stdint.h>

shared int failures = 0;

#define DECL_TEST(_type,_code)                              \
  void test_##_code(void) {                                 \
    static shared _type var = 1;                            \
    _type local_var;                                        \
    int want = (THREADS * (THREADS - 1)) * 2;               \
                                                            \
    if (!MYTHREAD) {                                        \
     local_var = bupc_atomic##_code##_fetchadd_relaxed(&var, 1); \
     if (local_var != 1) {                                  \
      printf("ERROR: test_" #_code "() thread 0 read %d when expecting 1\n", \
             (int)local_var);                               \
      ++failures;                                           \
     }                                                      \
     local_var = bupc_atomic##_code##_fetchadd_strict(&var, 1); \
     if (local_var != 2) {                                  \
      printf("ERROR: test_" #_code "() thread 0 read %d when expecting 2\n", \
             (int)local_var);                               \
      ++failures;                                           \
     }                                                      \
    }                                                       \
                                                            \
    upc_barrier;                                            \
                                                            \
    bupc_atomic##_code##_set_relaxed(&var, 0);              \
    bupc_atomic##_code##_set_strict(&var, 0);               \
                                                            \
    upc_barrier;                                            \
                                                            \
    bupc_atomic##_code##_fetchadd_relaxed(&var, MYTHREAD);  \
    bupc_atomic##_code##_fetchadd_strict(&var, MYTHREAD);   \
                                                            \
    upc_barrier;                                            \
                                                            \
    local_var = bupc_atomic##_code##_read_relaxed(&var) +   \
                bupc_atomic##_code##_read_strict(&var);     \
    if (local_var != want) {                                \
      printf("ERROR: test_" #_code "() thread %d read %d when expecting %d\n", \
             MYTHREAD, (int)local_var, want);               \
      ++failures;                                           \
    }                                                       \
  }

DECL_TEST(uint64_t, U64)
DECL_TEST(int64_t,  I64)

int main(void) {
   
  test_U64();
  test_I64();

  upc_barrier;

  if (!MYTHREAD) {
    printf(failures ? "FAILURE\n" : "SUCCESS\n");
  }

  upc_barrier;

  return !!failures;
}
