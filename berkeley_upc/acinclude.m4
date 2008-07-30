AC_DEFUN([cv_prefix],[upcr_cv_])

dnl Berkeley UPC package version number
dnl See README.developers for the version numbering scheme
AC_DEFUN([UPCR_VERSION_LIT],[2.6.0])

AC_DEFUN([UPCR_INIT_VERSIONS], [

  dnl set this to default translator for this release
  default_translator="http://upc-translator.lbl.gov/upcc-2.6.0.cgi"

  dnl version of the runtime spec adhered to
  UPCR_RUNTIME_SPEC_MAJOR=3
  UPCR_RUNTIME_SPEC_MINOR=9
  RUNTIME_SPEC="${UPCR_RUNTIME_SPEC_MAJOR}.${UPCR_RUNTIME_SPEC_MINOR}"

  dnl UPC language specification identifier (ie __UPC_VERSION__)
  UPC_SPEC="200505L"

  UPCR_VERSION=UPCR_VERSION_LIT
  AC_DEFINE_UNQUOTED(UPCR_VERSION,"$UPCR_VERSION")
  AC_SUBST(UPCR_VERSION)
  AC_SUBST(RUNTIME_SPEC)
  AC_SUBST(UPC_SPEC)
  AC_DEFINE_UNQUOTED(UPCR_RUNTIME_SPEC_MAJOR,$UPCR_RUNTIME_SPEC_MAJOR)
  AC_DEFINE_UNQUOTED(UPCR_RUNTIME_SPEC_MINOR,$UPCR_RUNTIME_SPEC_MINOR)
])

dnl code to run very early in configure
AC_DEFUN([UPCR_EARLY_INIT],[
  # default multiconf setting:
  upcri_multiconf=1
  for upcri_arg in "[$]@"; do
    case "$upcri_arg" in
      --with-multiconf-magic=* | -with-multiconf-magic=*) 
        upcri_multiconf_magic=1 ;;
      --with-multiconf* | -with-multiconf*) 
        upcri_multiconf=1 ;;
      --without-multiconf | -without-multiconf) 
        upcri_multiconf=0 ;;
      --reboot | -reboot) 
        upcri_reboot=1 ;;
      -help | --help | --hel | --he | -h | -help=r* | --help=r* | --hel=r* | --he=r* | -hr* | -help=s* | --help=s* | --hel=s* | --he=s* | -hs*)
        upcri_help=1 ;;
    esac
  done
  if ( test "$upcri_multiconf" = "1" || test "$upcri_help" = "1" || test "$upcri_reboot" = "1" ) \
     && test "$upcri_multiconf_magic" != "1" ; then
    # find a reasonable perl
    for upcri_perl in $PERL perl /usr/bin/perl /bin/perl /usr/local/bin/perl ; do
      if test "`$upcri_perl -v 2>&1 > /dev/null`" = "" ; then
        break
      fi
    done
    if test "[$]0" = "configure" ; then
      srcdir="."
    else
      srcdir=`echo "[$]0" | "$upcri_perl" -pe 's@/[[^/]]+[$]@@'`
    fi
    if test "$upcri_help" = "1" ; then
      exec "$upcri_perl" "$srcdir/multiconf.pl" --help "[$]@"
    elif test "$upcri_reboot" = "1" && test "$upcri_multiconf" != "1"; then
      set -x
      exec "$upcri_perl" "$srcdir/multiconf.pl" --reboot --without-multiconf "[$]@"
    else
      echo "Starting multiconf..."
      set -x
      exec "$upcri_perl" "$srcdir/multiconf.pl" "[$]@"
      echo "ERROR: failed to exec multiconf" 1>&2
      exit 1
    fi
  fi
  if test "$upcri_help" != "1" && test "$upcri_hello_shown" != 1; then
    # avoid duplicate messages caused by configure re-execing itself (eg on Tru64)
    upcri_hello_shown=1
    export upcri_hello_shown
    echo 'Configuring Berkeley UPC Runtime version UPCR_VERSION_LIT with the following options:'
    echo "  " "[$]@"
  fi
])

dnl co-opt the AC_REVISION mechanism to run some code very early
AC_REVISION([
UPCR_EARLY_INIT
dnl swallow any autoconf-provided revision suffix: 
echo > /dev/null \
])

