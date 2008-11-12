# Compiler spec file for Intrepid GCC/UPC 4.x, stand-alone version

# upc compiler command
upc_compiler = /eng/upc/gcc-upc-4-2/wrk/gcc/xupc -I. -g -Wno-implicit-function-declaration

# upc run command
# Following replacements are performed:
# %N - number of UPC threads
# %P - program executable
# %A - arguments to program
# %B - berkeley-specific upcrun arguments (should appear if and only if this is Berkeley upcrun)
upcrun_command = %P -fupc-threads-%N %A

# default sysconf file to use
# -network setting is ignored for non-UPCR compilers, 
# just need a sysconf with a relevant batch policy for job execution
default_sysconf = shmem-interactive

# list of supported compiler features
feature_list = gccupc,os_linux,cpu_x86_64,cpu_64,cc_gnu

# option to pass upc compiler to get %T static threads
upc_static_threads_option = -fupc-threads-%T

# option for performing just the source-to-source compile step
# or empty if not supported by the compiler
upc_trans_option = 

# colon-delimited path where to find harness.conf files
suite_path = $TOP_SRCDIR$/upc-tests:$TOP_SRCDIR$/upc-examples

# GNU make
gmake = make

# misc system tools
ar = ar
ranlib = ranlib

# C compiler & flags (should be empty on upcr/GASNet to auto-detect)
cc = gcc
cflags =
ldflags = 
libs =

# host C compiler (or empty for same as cc)
host_cc = 
host_cflags =
host_ldflags =
host_libs = 

# known failures, in the format: test-path/test-name[failure comment] | test-name[failure comment]
# lines may be broken using backslash
# known_failures may be empty to use the ones in the harness.conf files
known_failures = \
bug175a [GCC/UPC doesnt calculate sizeof() properly on certain shared types] | \
bug247 [test checks to see if 256 threads are supported, but GCC/UPC supports only 255 threads on 32 bit targets ] | \
bug317a [GCC/UPC doesnt support implicit conversions between integers and shared pointers] | \
bug342 [GCC/UPC doesn\'t suppress the warning about __transparent_union__] | \
bug383 [GCC/UPC warns about unprototyped function call] | \
bug438 [compile-warning;gccupc;GCC/UPC warns C99 inline functions are not supported] | \
bug846 [GCC/UPC doesn\'t accept large arrays] | \
bug851 [test expects returns with no value to be diagnosed as errors, but GCC/UPC issues warnings in those cases] | \
bug856 [Problem with upc_blocksizeof calculation - fails at runtime] | \
bug856b [Problem with upc_blocksizeof calculation - fails at runtime] | \
bug866-5 [Problem with upc_blocksizeof calculation - fails at runtime] | \
bug899 [GCC/UPC doesn\'t properly handle #pragma upc c_code, by undefining UPC keywords and reserved identifiers] | \
bug899b [GCC/UPC doesn\'t properly handle #pragma upc c_code, by undefining UPC keywords and reserved identifiers] | \
bug899b [GCC/UPC doesn\'t properly handle #pragma upc c_code, by undefining UPC keywords and reserved identifiers] | \
bug922 [GCC/UPC doesn\'t properly handle #pragma upc c_code, by undefining UPC keywords and reserved identifiers] | \
bug1223 [bug1223 redefines system types, causing conflict failures with implicit includes] | \
bug1578 [Issues (expected) error on passing actual (shared *) to declared (shared \[\] *), and silently ignores presence of shared \[\] in prototype while routine declaration does not supply a blocking factor] | \
bug1639 [Silently ignores presence of shared \[\] in prototype while routine declaration does not supply a blocking factor] | \
bug1668 [Silently performs addition on (shared void *) operand.  Should issue error diagnostic] | \
checkforall [GCC/UPC does not allow static initializers which reference the address expressions involving the address of a shared variable] | \
lockperf [Refers to Berkeley specific bupc_collectivev.h header file] | \
touchperf [Refers to Berkeley specific bupc_tick_t type] | \
tricky_sizeof4 [compiler isnt diagnosiing upc_localsizeof (void) as an error] | \
tricky_sizeof5 [fails when calculating the size of a shared array with a THREADS factor, when compiled in a dynamic THREADS environment] | \
tricky_sizeof6-dynamic [supposed to fail at compile-time but doesnt] | \
tricky_sizeof7 [test tries taking sizeof(void), which should be an error, but compiler fails to diagnose it] | \
p1 [Some backend C compilers cannot handle spaces in file names] | \
p2 [cannot handle pathnames containing special punctuation characters]
