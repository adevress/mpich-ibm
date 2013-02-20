## -*- Mode: Makefile; -*-
## vim: set ft=automake :
##
## (C) 2011 by Argonne National Laboratory.
##     See COPYRIGHT in top-level directory.
##

bin_SCRIPTS +=           \
    src/env/mpicc        \
    src/env/parkill

bin_PROGRAMS += src/env/mpich2version

src_env_mpich2version_SOURCES = src/env/mpich2version.c
src_env_mpich2version_LDADD = lib/lib@MPILIBNAME@.la
if BUILD_PROFILING_LIB
src_env_mpich2version_LDADD += lib/lib@PMPILIBNAME@.la
endif BUILD_PROFILING_LIB

src_env_mpich2version_LDFLAGS = $(mpich_libtool_static_flag)

sysconf_DATA += src/env/mpicc.conf

if BUILD_F77_BINDING
bin_SCRIPTS += src/env/mpif77
sysconf_DATA += src/env/mpif77.conf
endif BUILD_F77_BINDING

if BUILD_F90_LIB
bin_SCRIPTS += src/env/mpif90 src/env/mpif95 src/env/mpif2003
sysconf_DATA += src/env/mpif90.conf src/env/mpif95.conf src/env/mpif2003.conf
endif BUILD_F90_LIB

if BUILD_CXX_LIB
bin_SCRIPTS += src/env/mpicxx
sysconf_DATA += src/env/mpicxx.conf
endif BUILD_CXX_LIB

# create a local copy of the compiler wrapper that will actually be installed
if BUILD_BASH_SCRIPTS
src/env/mpicc: $(top_builddir)/src/env/mpicc.bash
	cp -p $? $@
src/env/mpicxx: $(top_builddir)/src/env/mpicxx.bash
	cp -p $? $@
src/env/mpif77: $(top_builddir)/src/env/mpif77.bash
	cp -p $? $@
src/env/mpif90: $(top_builddir)/src/env/mpif90.bash
	cp -p $? $@
src/env/mpif95: $(top_builddir)/src/env/mpif95.bash
	cp -p $? $@
src/env/mpif2003: $(top_builddir)/src/env/mpif2003.bash
	cp -p $? $@
else !BUILD_BASH_SCRIPTS
src/env/mpicc: $(top_builddir)/src/env/mpicc.sh
	cp -p $? $@
src/env/mpicxx: $(top_builddir)/src/env/mpicxx.sh
	cp -p $? $@
src/env/mpif77: $(top_builddir)/src/env/mpif77.sh
	cp -p $? $@
src/env/mpif90: $(top_builddir)/src/env/mpif90.sh
	cp -p $? $@
src/env/mpif95: $(top_builddir)/src/env/mpif95.sh
	cp -p $? $@
src/env/mpif2003: $(top_builddir)/src/env/mpif2003.sh
	cp -p $? $@
endif !BUILD_BASH_SCRIPTS

DISTCLEANFILES += $(top_builddir)/src/env/cc_shlib.conf  \
                  $(top_builddir)/src/env/cxx_shlib.conf \
                  $(top_builddir)/src/env/f77_shlib.conf \
                  $(top_builddir)/src/env/fc_shlib.conf  \
                  $(top_builddir)/src/env/mpicc          \
                  $(top_builddir)/src/env/mpicxx         \
                  $(top_builddir)/src/env/mpif77         \
                  $(top_builddir)/src/env/mpif90         \
                  $(top_builddir)/src/env/mpif95         \
                  $(top_builddir)/src/env/mpif2003

wrapper_doc_src = src/env/mpicc.txt \
                  src/env/mpif77.txt \
                  src/env/mpicxx.txt \
                  src/env/mpif90.txt \
                  src/env/mpif95.txt \
                  src/env/mpif2003.txt \
                  src/env/mpiexec.txt
doc1_src_txt += $(wrapper_doc_src)
EXTRA_DIST += $(wrapper_doc_src)

