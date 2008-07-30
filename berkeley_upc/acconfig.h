/*
 * UPC Runtime common configuration template--parsed though autoconf 
 * to generate upcr_config.h.
 *  -- note: base file is 'acconfig.h'--make any changes there!
 *
 * Notes: 
 *  -- All public symbols should use UPCR_ ("UPC Runtime") prefix.
 *  -- All internal symbols should use UPCRI_ ("UPC Runtime internal") prefix.
 *	 -- Preprocessor symbols w/o this are autoconf-generated.
 *  -- We rely heavily on the GASNet config to avoid duplication.
 *
 * $Id: acconfig.h,v 1.41 2006/08/31 01:21:11 bonachea Exp $
 */

#ifndef UPCR_CONFIG_H
#define UPCR_CONFIG_H

/* Everything above this is passed though autoconf verbatim */
@TOP@

#undef UPCR_VERSION
#undef UPCR_RUNTIME_SPEC_MAJOR
#undef UPCR_RUNTIME_SPEC_MINOR
#undef UPCRI_SYSTEM_NAME
#undef UPCRI_SYSTEM_TUPLE
#undef UPCRI_CONFIGURE_ARGS
#undef UPCRI_BUILD_ID

/* Whether this copy of the runtime is being used as a target for the gccupc
 * binary UPC compiler */
#undef UPCRI_USING_GCCUPC

/* Define if you wish to extend printf (and friends) with  %S/%P format 
 * specifiers for shared/pshared pointers (requires glibc) */
#undef UPCR_EXTEND_PRINTF

/* Whether or not to enable the GASP instrumentation hooks */
#undef UPCRI_GASP

/* Whether or not to use the more efficient 'packed' shared pointer representation */
#undef UPCRI_PACKED_SPTR

/* Whether or not to use the 'struct' shared pointer representation */
#undef UPCRI_STRUCT_SPTR

/* Whether or not to enable the pshared symmetric pointer variant of shared pointers */
#undef UPCRI_SYMMETRIC_SPTR

/* configure-selected bit distribution for the packed shared pointer representation */
#undef UPCRI_PHASE_BITS_OVERRIDE
#undef UPCRI_THREAD_BITS_OVERRIDE
#undef UPCRI_ADDR_BITS_OVERRIDE

/* have pthread_setconcurrency */
#undef HAVE_PTHREAD_SETCONCURRENCY

/* have new linux pthreads library (NPTL) */
#undef HAVE_PTHREADS_NPTL

/* gcc built-ins */
#undef HAVE_BUILTIN_PREFETCH
#undef HAVE_BUILTIN_HUGE_VAL
#undef HAVE_BUILTIN_HUGE_VALF
#undef HAVE_BUILTIN_INF
#undef HAVE_BUILTIN_INFF
#undef HAVE_BUILTIN_NAN
#undef HAVE_BUILTIN_NANF

/* have pthread_attr_setguardsize */
#undef HAVE_PTHREAD_ATTR_SETGUARDSIZE

/* __thread attribute works for pthread-local data */
#undef HAVE_GCC_TLS_SUPPORT

/* mcleanup() gprof function */
#undef HAVE_MCLEANUP

/* monitor_signal() Compaq profiling function */
#undef HAVE_MONITOR_SIGNAL

/* monitor(0) AIX profiling function */
#undef HAVE_MONITOR

/* monitor(0,0,0,0,0) Solaris profiling function */
#undef HAVE_MONITOR5

/* PGI rouexit profile cleanup function */
#undef HAVE_ROUEXIT

/* C program stack grows up */
#undef UPCRI_STACK_GROWS_UP

/* have attribute(cleanup) support */
#undef UPCRI_HAVE_ATTRIBUTE_CLEANUP

/* portable inttypes support */
#undef HAVE_INTTYPES_H
#undef HAVE_STDINT_H
#undef HAVE_SYS_TYPES_H
#undef COMPLETE_INTTYPES_H
#undef COMPLETE_STDINT_H
#undef COMPLETE_SYS_TYPES_H


/* Everything below this is passed though autoconf verbatim */
@BOTTOM@

/* Bug 1097: do NOT place any includes in this file - it should only contain definitions */

#endif /* UPCR_CONFIG_H */
