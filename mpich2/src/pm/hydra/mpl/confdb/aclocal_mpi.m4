dnl
dnl/*D 
dnl PAC_LIB_MPI - Check for MPI library
dnl
dnl Synopsis:
dnl PAC_LIB_MPI([action if found],[action if not found])
dnl
dnl Output Effect:
dnl
dnl Notes:
dnl Currently, only checks for lib mpi and mpi.h.  Later, we will add
dnl MPI_Pcontrol prototype (const int or not?).  
dnl
dnl Prerequisites:
dnl autoconf version 2.13 (for AC_SEARCH_LIBS)
dnl D*/
dnl Other tests to add:
dnl Version of MPI
dnl MPI-2 I/O?
dnl MPI-2 Spawn?
dnl MPI-2 RMA?
dnl PAC_LIB_MPI([found text],[not found text])
AC_DEFUN([PAC_LIB_MPI],[
dnl Set the prereq to 2.50 to avoid having 
AC_PREREQ(2.50)
if test "X$pac_lib_mpi_is_building" != "Xyes" ; then
  # Use CC if TESTCC is defined
  if test "X$pac_save_level" != "X" ; then
     pac_save_TESTCC="${TESTCC}"
     pac_save_TESTCPP="${TESTCPP}"
     CC="$pac_save_CC"
     if test "X$pac_save_CPP" != "X" ; then
         CPP="$pac_save_CPP"
     fi
  fi
  # Look for MPILIB first if it is defined
  AC_SEARCH_LIBS(MPI_Init,$MPILIB mpi mpich mpich2)
  if test "$ac_cv_search_MPI_Init" = "no" ; then
    ifelse($2,,
    AC_MSG_ERROR([Could not find MPI library]),[$2])
  fi
  AC_CHECK_HEADER(mpi.h,pac_have_mpi_h="yes",pac_have_mpi_h="no")
  if test $pac_have_mpi_h = "no" ; then
    ifelse($2,,
    AC_MSG_ERROR([Could not find mpi.h include file]),[$2])
  fi
  if test "X$pac_save_level" != "X" ; then
     CC="$pac_save_TESTCC"
     CPP="$pac_save_TESTCPP"
  fi
fi
ifelse($1,,,[$1])
])

dnl This should also set MPIRUN.
dnl
dnl/*D
dnl PAC_ARG_MPI_TYPES - Add command-line switches for different MPI 
dnl environments
dnl
dnl Synopsis:
dnl PAC_ARG_MPI_TYPES([default])
dnl
dnl Output Effects:
dnl Adds the following command line options to configure
dnl+ \-\-with\-mpich[=path] - MPICH.  'path' is the location of MPICH commands
dnl. \-\-with\-ibmmpi - IBM MPI
dnl. \-\-with\-lammpi[=path] - LAM/MPI
dnl. \-\-with\-mpichnt - MPICH NT
dnl- \-\-with\-sgimpi - SGI MPI
dnl If no type is selected, and a default ("mpich", "ibmmpi", or "sgimpi")
dnl is given, that type is used as if '--with-<default>' was given.
dnl
dnl Sets 'CC', 'F77', 'TESTCC', 'TESTF77', and 'MPILIBNAME'.  Does `not`
dnl perform an AC_SUBST for these values.
dnl Also sets 'MPIBOOT' and 'MPIUNBOOT'.  These are used to specify 
dnl programs that may need to be run before and after running MPI programs.
dnl For example, 'MPIBOOT' may start demons necessary to run MPI programs and
dnl 'MPIUNBOOT' will stop those demons.
dnl 
dnl The two forms of the compilers are to allow for tests of the compiler
dnl when the MPI version of the compiler creates executables that cannot
dnl be run on the local system (for example, the IBM SP, where executables
dnl created with mpcc will not run locally, but executables created
dnl with xlc may be used to discover properties of the compiler, such as
dnl the size of data types).
dnl
dnl Historical note:
dnl Some common autoconf tests, such as AC_CHECK_SIZEOF, used to require
dnl running a program.  But some MPI compilers (often really compilation 
dnl scripts) produced programs that could only be run with special commands,
dnl such as a batch submission system.  To allow these test programs to be
dnl run, a separate set of compiler variables, TESTCC, TESTF77, etc., 
dnl were defined.  However, in later versions of autoconf, it both became
dnl unnecessary to run programs for tests such as AC_CHECK_SIZEOF and 
dnl it became necessary to define CC etc. before invoking AC_PROG_CC (and
dnl the othe language compilers), because those commands now do much, much
dnl more than just determining the compiler.
dnl
dnl To address the change, we still define the TESTCC ect. compilers where
dnl possible to allow the use of AC_TRY_RUN when required, but we define
dnl the CC etc variables and do not define ac_cv_prog_CC etc., as these 
dnl cause autoconf to skip all of the other initialization code that 
dnl AC_PROG_CC etc. runs.  Note also that this command must occur before 
dnl AC_PROG_CC (or anything that might cause AC_PROG_CC to be invoked).
dnl
dnl See also:
dnl PAC_LANG_PUSH_COMPILERS, PAC_LIB_MPI
dnl D*/
AC_DEFUN([PAC_ARG_MPI_TYPES],[
PAC_ARG_MPI_KNOWN_TYPES
PAC_MPI_FIND_COMPILERS
PAC_MPI_CHECK_MPI_LIB
])
dnl
dnl To keep autoconf from prematurely invoking the compiler check scripts,
dnl we need a command that first sets the compilers and a separate one
dnl that makes any necessary checks for libraries
dnl
AC_DEFUN([PAC_ARG_MPI_KNOWN_TYPES],[
AC_ARG_WITH(mpich,
[--with-mpich=path  - Assume that we are building with MPICH],
ac_mpi_type=mpich)
# Allow MPICH2 as well as MPICH
AC_ARG_WITH(mpich2,
[--with-mpich=path  - Assume that we are building with MPICH],
ac_mpi_type=mpich)
AC_ARG_WITH(lammpi,
[--with-lammpi=path  - Assume that we are building with LAM/MPI],
ac_mpi_type=lammpi)
AC_ARG_WITH(ibmmpi,
[--with-ibmmpi    - Use the IBM SP implementation of MPI],
ac_mpi_type=ibmmpi)
AC_ARG_WITH(sgimpi,
[--with-sgimpi    - Use the SGI implementation of MPI],
ac_mpi_type=sgimpi)
AC_ARG_WITH(mpichnt,
[--with-mpichnt - Use MPICH for Windows NT ],
ac_mpi_type=mpichnt)
AC_ARG_WITH(mpi,
[--with-mpi=path    - Use an MPI implementation with compile scripts mpicc
                     and mpif77 in path/bin],ac_mpi_type=generic)

if test "X$ac_mpi_type" = "X" ; then
    if test "X$1" != "X" ; then
        ac_mpi_type=$1
    else
        ac_mpi_type=unknown
    fi
fi
if test "$ac_mpi_type" = "unknown" -a "$pac_lib_mpi_is_building" = "yes" ; then
    ac_mpi_type="mpich"
fi
])
AC_DEFUN([PAC_MPI_FIND_COMPILERS],[
# Set defaults
MPIRUN_NP="-np "
MPIEXEC_N="-n "
AC_SUBST(MPIRUN_NP)
AC_SUBST(MPIEXEC_N)
case $ac_mpi_type in
	mpich)
        dnl 
        dnl This isn't correct.  It should try to get the underlying compiler
        dnl from the mpicc and mpif77 scripts or mpireconfig
        if test "X$pac_lib_mpi_is_building" != "Xyes" ; then
            save_PATH="$PATH"
            if test "$with_mpich" != "yes" -a "$with_mpich" != "no" ; then 
		# Look for commands; if not found, try adding bin to the
		# path
		if test ! -x $with_mpich/mpicc -a -x $with_mpich/bin/mpicc ; then
			with_mpich="$with_mpich/bin"
		fi
                PATH=$with_mpich:${PATH}
            fi
            AC_PATH_PROG(MPICC,mpicc)
            TESTCC=${CC-cc}
            CC="$MPICC"
            AC_PATH_PROG(MPIF77,mpif77)
            TESTF77=${F77-f77}
            F77="$MPIF77"
            AC_PATH_PROG(MPIFC,mpif90)
            TESTFC=${FC-f90}
            FC="$MPIFC"
            AC_PATH_PROG(MPICXX,mpiCC)
            TESTCXX=${CXX-CC}
            CXX="$MPICXX"
	    # We may want to restrict this to the path containing mpirun
	    AC_PATH_PROG(MPIEXEC,mpiexec)
	    AC_PATH_PROG(MPIRUN,mpirun)
	    AC_PATH_PROG(MPIBOOT,mpichboot)
	    AC_PATH_PROG(MPIUNBOOT,mpichstop)
	    PATH="$save_PATH"
  	    MPILIBNAME="mpich"
        else 
	    # All of the above should have been passed in the environment!
	    :
        fi
	;;

        mpichnt)
        ;;

	lammpi)
	dnl
        dnl This isn't correct.  It should try to get the underlying compiler
        dnl from the mpicc and mpif77 scripts or mpireconfig
        save_PATH="$PATH"
        if test "$with_mpich" != "yes" -a "$with_mpich" != "no" ; then 
	    # Look for commands; if not found, try adding bin to the path
		if test ! -x $with_lammpi/mpicc -a -x $with_lammpi/bin/mpicc ; then
			with_lammpi="$with_lammpi/bin"
		fi
                PATH=$with_lammpi:${PATH}
        fi
        AC_PATH_PROG(MPICC,mpicc)
        TESTCC=${CC-cc}
        CC="$MPICC"
        AC_PATH_PROG(MPIF77,mpif77)
        TESTF77=${F77-f77}
        F77="$MPIF77"
        AC_PATH_PROG(MPIFC,mpif90)
        TESTFC=${FC-f90}
        FC="$MPIFC"
        AC_PATH_PROG(MPICXX,mpiCC)
        TESTCXX=${CXX-CC}
        CXX="$MPICXX"
	PATH="$save_PATH"
  	MPILIBNAME="lammpi"
	MPIBOOT="lamboot"
	MPIUNBOOT="wipe"
	MPIRUN="mpirun"
	;;

	ibmmpi)
	AC_CHECK_PROGS(MPCC,mpcc)
	AC_CHECK_PROGS(MPXLF,mpxlf)
	if test -z "$MPCC" -o -z "$MPXLF" ; then
	    AC_MSG_ERROR([Could not find IBM MPI compilation scripts.  Either mpcc or mpxlf is missing])
	fi
	TESTCC=${CC-xlC}; TESTF77=${F77-xlf}; CC=mpcc; F77=mpxlf
	# There is no mpxlf90, but the options langlvl and free can
	# select the Fortran 90 version of xlf
	if test "$enable_f90" != no ; then
	    AC_CHECK_PROGS(MPIXLF90,mpxlf90)
	    TESTFC=${FC-xlf90}
            if test "X$MPIXLF90" != "X" ; then 
	        FC="mpxlf90"
	    else
	    	FC="mpxlf -qlanglvl=90ext -qfree=f90"
	    fi
	fi
	MPILIBNAME=""
	cross_compiling=yes
	# Turn off the autoconf version 3 warning message
	ac_tool_warned=yes
	;;

	sgimpi)
	TESTCC=${CC:=cc}; TESTF77=${F77:=f77}; 
	TESTCXX=${CXX:=CC}; TESTFC=${FC:=f90}
	MPIRUN=mpirun
	MPIBOOT=""
	MPIUNBOOT=""
	;;

	generic)
	# Find the compilers.  Expect the compilers to be mpicc and mpif77
	# in $with_mpi/bin or $with_mpi
        if test "X$MPICC" = "X" ; then
            if test -x "$with_mpi/bin/mpicc" ; then
                MPICC=$with_mpi/bin/mpicc
	    elif test -x "$with_mpi/mpicc" ; then
	        MPICC=$with_mpi/mpicc
            fi
        fi
        if test "X$MPICXX" = "X" ; then
            if test -x "$with_mpi/bin/mpicxx" ; then
                MPICXX=$with_mpi/bin/mpicxx
	    elif test -x "$with_mpi/mpicxx" ; then
	        MPICXX=$with_mpi/mpicxx
            fi
        fi
        if test "X$MPIF77" = "X" ; then
            if test -x "$with_mpi/bin/mpif77" ; then
                MPIF77=$with_mpi/bin/mpif77
	    elif test -x "$with_mpi/mpif77" ; then
	        MPIF77=$with_mpi/mpif77
            fi
        fi
        if test "X$MPIF90" = "X" ; then
            if test -x "$with_mpi/bin/mpif90" ; then
                MPIF90=$with_mpi/bin/mpif90
	    elif test -x "$with_mpi/mpif90" ; then
	        MPIF90=$with_mpi/mpif90
            fi
        fi
        if test "X$MPIEXEC" = "X" ; then
            if test -x "$with_mpi/bin/mpiexec" ; then
                MPIEXEC=$with_mpi/bin/mpiexec
	    elif test -x "$with_mpi/mpiexec" ; then
	        MPIEXEC=$with_mpi/mpiexec
            fi
        fi
        CC=$MPICC
        F77=$MPIF77
	if test "X$MPICXX" != "X" ; then CXX=$MPICXX ; fi
	if test "X$MPIF90" !- "X" ; then F90=$MPIF90 ; fi
	;;

	*)
	# Use the default choices for the compilers
	;;
esac
# Tell autoconf to determine properties of the compilers (these are the 
# compilers for MPI programs)
AC_PROG_CC
if test "$enable_f77" != no -a "$enable_fortran" != no ; then
    AC_PROG_F77
fi
if test "$enable_cxx" != no ; then
    AC_PROG_CXX
fi
if test "$enable_f90" != no ; then
    PAC_PROG_FC
fi
])

dnl
dnl This uses the selected CC etc to check for include paths and libraries
AC_DEFUN([PAC_MPI_CHECK_MPI_LIB],[
case $ac_mpi_type in
	mpich)
	;;

        mpichnt)
        dnl
        dnl This isn't adequate, but it helps with using MPICH-NT/SDK.gcc
	save_CFLAGS="$CFLAGS"
        CFLAGS="$save_CFLAGS -I$with_mpichnt/include"
        save_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$save_CPPFLAGS -I$with_mpichnt/include"
        save_LDFLAGS="$LDFLAGS"
        LDFLAGS="$save_LDFLAGS -L$with_mpichnt/lib"
        AC_CHECK_LIB(mpich,MPI_Init,found="yes",found="no")
        if test "$found" = "no" ; then
          AC_CHECK_LIB(mpich2,MPI_Init,found="yes",found="no")
        fi
        if test "$found" = "no" ; then
          CFLAGS=$save_CFLAGS
          CPPFLAGS=$save_CPPFLAGS
          LDFLAGS=$save_LDFLAGS
        fi
        ;;

	lammpi)
	;;

	ibmmpi)
	;;

	sgimpi)
	AC_CHECK_LIB(mpi,MPI_Init)
	if test "$ac_cv_lib_mpi_MPI_Init" = "yes" ; then
	    MPILIBNAME="mpi"
	fi	
	;;

	generic)
	AC_SEARCH_LIBS(MPI_Init,mpi mpich2 mpich)
	if test "$ac_cv_lib_mpi_MPI_Init" = "yes" ; then
	    MPILIBNAME="mpi"
	fi	
	;;

	*)
	;;
esac
])

dnl
dnl/*D
dnl PAC_MPI_F2C - Determine if MPI has the MPI-2 functions MPI_xxx_f2c and
dnl   MPI_xxx_c2f
dnl
dnl Output Effect:
dnl Define 'HAVE_MPI_F2C' if the routines are found.
dnl
dnl Notes:
dnl Looks only for 'MPI_Request_c2f'.
dnl D*/
AC_DEFUN([PAC_MPI_F2C],[
AC_CACHE_CHECK([for MPI F2C and C2F routines],
pac_cv_mpi_f2c,
[
AC_TRY_LINK([#include "mpi.h"],
[MPI_Request request;MPI_Fint a;a = MPI_Request_c2f(request);],
pac_cv_mpi_f2c="yes",pac_cv_mpi_f2c="no")
])
if test "$pac_cv_mpi_f2c" = "yes" ; then 
    AC_DEFINE(HAVE_MPI_F2C,1,[Define if MPI has F2C]) 
fi
])
dnl
dnl/*D
dnl PAC_HAVE_ROMIO - make mpi.h include mpio.h if romio enabled
dnl
dnl Output Effect:
dnl expands @HAVE_ROMIO@ in mpi.h into #include "mpio.h"
dnl D*/
AC_DEFUN([PAC_HAVE_ROMIO],[
if test "$enable_romio" = "yes" ; then HAVE_ROMIO='#include "mpio.h"'; fi
AC_SUBST(HAVE_ROMIO)
])
