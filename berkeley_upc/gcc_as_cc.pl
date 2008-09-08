#!/usr/bin/perl

#
# A program to figure out how to make the GCC preprocessor ape cc's
#

my $verbose = 0;  # set to 1 when debugging


use strict;
use Getopt::Long;

my $tmpfile = "tmpfoo.c";
my $outfile = "tmpfoo.out";
my ($gcc, $cc, $cc_family);

exit(-1) unless GetOptions(
    'gcc=s'            => \$gcc,
    'cc=s'             => \$cc,
    'cc-family=s'      => \$cc_family,
    'v'                => \$verbose
);

die "usage: $0 --gcc=<gcc -E> --cc=<cc -E> --cc-family=<cc compiler family>\n"
    unless $gcc && $cc && $cc_family;

system("echo 'int x = 4;' > $tmpfile") and die "Could't create '$tmpfile'\n";

# sanity checks
foreach my $comp ($gcc, $cc) {
  my $out = `$comp $tmpfile 2>&1 > /dev/null`;
  $out =~ s/^$tmpfile.?\s*$//;
  $out =~ s/^.*?: INFO: .*\n//; # silly Cray target message on XT4
  if ($out) {
    die "$out\nERROR: '$comp' does not appear to be a functional C preprocessor!\n";
  }
}

my @may_defs;

# find the system include paths
my @isys;
if ($cc_family =~ m/^xlc$/i) { # IBM VisualAge C
  my $xlcentry=`$cc -v $tmpfile 2>&1`;
  if ($xlcentry =~ m/(xlcentry|xlCcpp)\((.*?)\)/) {
    my @xlcentryopts = split(/,/,$2);
    verbose("got xlcentry:\n".join("\n  ",@xlcentryopts)."\n");
    for my $incvar ("c_stdinc", "gcc_c_stdinc") {
      for my $opt (@xlcentryopts) {
        if ($opt =~ m/^-q$incvar=(.*)$/) {
          push(@isys, split(/:/, $1));
        } elsif ($opt =~ m/^-D(.*)$/) {
	  my $def = $1;
          $def =~ s/["']//g;
          $def =~ s/=(.*)$//g;
          push(@may_defs, $def);
        }
      }
    }
  } 
  if (!scalar @isys) {
    verbose("using /usr/include as system dir\n");
    push(@isys,"/usr/include");
  }
} elsif ($cc_family =~ m/^Sun$/i) { # Sun CC
  push(@isys,"/usr/include");
  # cc -# is verbose compile
  my $voutput = `$cc -# $tmpfile 2>&1 > /dev/null`;
  if ($voutput =~ m/acomp (.*)$/) {
    my @acompopts = split(/ /,$1);
    verbose("got acompopts:\n".join("\n  ",@acompopts)."\n");
    for my $opt (@acompopts) {
      if ($opt =~ m/^-I(.*)$/) {
          push(@isys, $1);
      } elsif ($opt =~ m/^-D(.*)$/) {
	  my $def = $1;
          $def =~ s/["']//g;
          $def =~ s/=(.*)$//g;
          push(@may_defs, $def);
      }
    }
  } else {
    verbose("failed to find system dirs - using /usr/include alone\n");
  }
} elsif ($cc_family =~ m/^PGI$/i) { # Portland Group C
  my $voutput = `$cc -silent -v $tmpfile 2>&1 > /dev/null`;
  if ($voutput =~ m/pgc (.*)\n/) {
    my @pgcopts = split(/ /,$1);
    verbose("got pgcopts:\n".join("\n  ",@pgcopts)."\n");
    for (my $i = 0; $i < @pgcopts; $i++) {
      if ($pgcopts[$i] =~ m/^-idir$/) { # PGC 6.0
	push(@isys, $pgcopts[$i+1]);
        $i++;
      } elsif ($pgcopts[$i] =~ m/^-stdinc$/) { # PGC 5.1
        my $dirs = $pgcopts[$i+1];
	push(@isys, split(/:/, $dirs));
        $i++;
      } elsif ($pgcopts[$i] =~ m/^-def$/) { # find some preprocessor defs
	my $def = $pgcopts[$i+1];
	$def =~ s/["']//g;
	$def =~ s/=(.*)$//g;
	push(@may_defs, $def);
        $i++;
      }
    }
  } else {
    verbose("using /usr/include as system dir\n");
    push(@isys,"/usr/include");
  }
} elsif ($cc_family =~ m/^Compaq$/i) { # Compaq C
  my $voutput = `$cc -v $tmpfile 2>&1 > /dev/null`;
  my $opts;
  if ($voutput =~ m/\/gemc_cc (.*)$/m)   { $opts .= "$1 "; }
  if ($voutput =~ m/^(.*?-D__DECC.*)$/m) { $opts .= "$1 "; }
  if ($opts) {
    my @ccopts = split(/ /,$opts);
    verbose("got ccopts:\n".join("\n  ",@ccopts)."\n");
    for (my $i = 0; $i < @ccopts; $i++) {
      if ($ccopts[$i] =~ m/^-I(\/.+)$/) { 
	push(@isys, $1);
      } elsif ($ccopts[$i] =~ m/^-D(.+)$/) { # find some preprocessor defs
	my $def = $1;
	$def =~ s/["']//g;
	$def =~ s/=(.*)$//g;
	push(@may_defs, $def) unless ($def eq "__TIME__" || $def eq "__DATE__");
      }
    }
    push(@isys,"/usr/include") unless (grep /^\/usr\/include$/,@isys); # /usr/include is implicit
  } else {
    verbose("using /usr/include as system dir\n");
    push(@isys,"/usr/include");
  }
} else { die "unknown --cc-family=$cc_family\n"; }
my $sysdirs = "-nostdinc " . join(' ',map { "-isystem $_" } @isys);
verbose("using sysdirs: $sysdirs\n");

my $gcc_tweaked = $gcc;

# test for -no-gcc, option in pre-3.4 that prevents __GNUC__, etc.
my $nogcc_out = `$gcc -no-gcc $tmpfile 2>&1 > /dev/null`;
if ($nogcc_out) {
  chomp($nogcc_out);
  verbose("gcc does not appear to support -no-gcc: $nogcc_out\n");
} else {
  verbose("gcc does appear to support -no-gcc\n");
  $gcc_tweaked .= " -no-gcc";  
}

$gcc_tweaked .= " $sysdirs";

my (%gcc_defines, @gcc_keep);

my $rawgcc = `$gcc_tweaked -dM $tmpfile 2>/dev/null`;
my @lines = split('\n', $rawgcc);
for my $line (@lines) {
    if ($line =~ /#define\s+(\S+)\s*(.*?)\s*$/) {
        $gcc_defines{$1} = $2;
    }
}
if (!defined $gcc_defines{"__STDC__"}) { # gcc does define __STDC__, but -dM lies about it
  $gcc_defines{"__STDC__"} = 1;
}

for my $def (sort keys %gcc_defines) {
    open(TESTFILE, ">$tmpfile") or die "Can't open '$tmpfile' for write\n";
    print TESTFILE <<EOF;
#ifndef $def
#error not defined
#else
cc sez "$def"=$def
#endif
int main() {}
EOF
    close(TESTFILE);
    if (system("$cc $tmpfile 2>/dev/null >$outfile")) {
        if ($? & 127) { # got a signal
          die "$cc failed with a signal.\n";
        }
        # cc doesn't define, so undefine in gcc
        verbose("-\t\t\t\tundefining GCC's $def\n");
        $gcc_tweaked .= " -U$def";
    } else {
        open (OUTFILE, $outfile) or die "Can't open $outfile\n";
        while (<OUTFILE>) {
            if (/cc sez "$def"\s*=\s*(.*?)\s*$/) {
		my $val = $1;
                push @gcc_keep, $def;
		if ($val ne $gcc_defines{$def}) {
                  # Make sure gcc uses same value
		  check_change();
                  $gcc_tweaked .= qq< -U$def -D$def="$val">;
                  verbose("~changing GCC's $def=\"".$gcc_defines{$def}."\" to \"$val\"\n");
		} else {
                  verbose("=keeping GCC's $def=\"$val\"\n");
		}
		last;
            }
        }
        close(OUTFILE);
    }
}

# now see which possible cc defs are being used
my @cc_maybes;
if ($cc_family =~ m/^xlc$/i) { # IBM VisualAge C
  # list from IBM "C for AIX User's Guide", pp 360-1
  @cc_maybes = (qw<__64BIT__ _AIX32 _AIX41 _AIX43 _AIX51 _AIX52 __ANSI__ _CHAR_SIGNED
                  _CHAR_UNSIGNED __CLASSIC__ __EXTENDED__ __HOS_AIX
                  __IBMC__ __IBMSMP _ILP32 _LONG_LONG _LONGDOUBLE128
                  _LP64 __MATH__ _OPENMP _POWER __SAA__ __SAAL2__ __STR__
                  __THW_INTEL__ __THW_RS6000__ __xlC__ __XLC21__
                  _BIG_ENDIAN __BIG_ENDIAN__ _CALL_SYSV __OPTIMIZE__ __OPTIMIZE_SIZE__
		  __powerpc __powerpc__ __powerpc64__ __PPC __PPC__ __ppc64 __PPC64__
                  __PTRDIFF_TYPE__ __SIZE_TYPE__ __unix __unix__
                  __ELF__ __TOS_LINUX__ __HOS_LINUX__ __linux __linux__
                  __GXX_WEAK__ _GNU_SOURCE
                >);
} elsif ($cc_family =~ m/^Sun$/i) { # Sun CC
  # list from "Sun C User's Guide - Sun WorkShop 6 update 2, July 2001, rev A"
  @cc_maybes = (qw<__STDC__ sun unix sparc i386
                   __sun __unix __sparc __i386 __SUNPRO_C
                   __BUILTIN_VA_ARG_INCR __SVR4 __sparcv9
                   __PRAGMA_REDEFINE_EXTNAME __RESTRICT 
                   __C99FEATURES__ __FLT_EVAL_METHOD__ __SUN_PREFETCH
                >);
  my $os = `uname -s`;
  my $osver = `uname -r`;
  chomp($os);
  chomp($osver);
  $osver =~ s/\./_/g;
  my $platsym = "__${os}_${osver}"; # (example: __SunOS_5_7)
  push(@cc_maybes, $platsym);
} elsif ($cc_family =~ m/^PGI$/i) { # Portland Group C
  # I can't find a documented list of predefined PGI macros anywhere...
  # these are all experimentally determined or taken from headers
  @cc_maybes = (qw<__STDC__ _OPENMP
                   __PGI __PGIC__ __PGIC_MINOR__ __PGIC_PATCHLEVEL__ 
		   _PGI_NOBUILTINS __PGI_LIBC __PGI_GNUC_VA_LIST PGI_DEBUG>);
} elsif ($cc_family =~ m/^Compaq$/i) { # Compaq C
  # list from "Tru64 UNIX Compaq C Language Reference Manual, AA-RH9NE-TE, September 2002"
  # cc -E also lets through some cc-specific #pragmas, which gcc discards, but that should be ok
  @cc_maybes = (qw< __STDC__  __STDC_VERSION__ __STDC_HOSTED__ __STDC_ISO_10646__
  		    __DECC __DECC_MODE_RELAXED __DECC_MODE_STRICT __DECC_MODE_COMMON __DECC_VER 
		    LANGUAGE_C __LANGUAGE_C__
		    __ALPHA __Alpha_AXP __INITIAL_POINTER_SIZE __PRAGMA_ENVIRONMENT 
                    unix __unix__ __osf SYSTYPE_BSD _SYSTYPE_BSD __alpha
		    __D_FLOAT __G_FLOAT __IEEE_FLOAT __X_FLOAT __HIDE_FORBIDDEN_NAMES
		    _XOPEN_SOURCE_EXTENDED _XOPEN_SOURCE _POSIX_C_SOURCE _ANSI_C_SOURCE 
		    _AES_SOURCE _OSF_SOURCE
		  >);
} else { die "unknown --cc-family=$cc_family\n"; }

push(@cc_maybes,@may_defs);
@cc_maybes = uniquify(@cc_maybes);
@cc_maybes = sort(@cc_maybes);

# filter out defs we've already tested from gcc
my @cc_check = grep { my $d = $_;
                       !grep /$d/, @gcc_keep; } @cc_maybes;

for my $def (@cc_check) {
    open(TESTFILE, ">$tmpfile") or die "Can't open '$tmpfile' for write\n";
    print TESTFILE <<EOF;
#ifndef $def
#error not defined
#else
cc sez "$def"=$def
#endif
int main() {}
EOF
    close(TESTFILE);
    if (system("$cc $tmpfile 2>/dev/null >$outfile")) {
        if ($? & 127) { # got a signal
          die "$cc failed with a signal.\n";
        }
        verbose("-\t\t\t\tnot using cc's $def\n"); 
    } else {
        open (OUTFILE, $outfile) or die "Can't open $outfile\n";
        while (<OUTFILE>) {
            if (/cc sez "$def"\s*=\s*(.*?)\s*$/) {
                # Make gcc define it
                $gcc_tweaked .= qq< -D$def="$1">;
                verbose("+adding cc's $def=\"$1\"\n"); 
                last;
            }
        }
        close(OUTFILE);
    }
}

my $extradefs = "";

if ($cc_family =~ m/^xlc$/i) { # IBM VisualAge C
  # the _Builtin keyword appears in several of xlc 7.0's headers, notably in stdarg.h
  $extradefs .= ' -D_Builtin=';

  # Problem: xlc for Linux defines the following macros:
  # __GNUC__ __GNUC_MINOR__
  # only while processing header files that are in a directory specified by 
  # -qgcc_c_stdinc or -qgcc_cpp_stdinc (ie the system directories)
  # we have no way to emulate this behavior, so we have to approximate it with a -D option (overkill)
  # otherwise PPC/Linux sys/types.h will think we lack the long long type and drop int64_t
  if (`uname -s 2> /dev/null` =~ /Linux/) {
    $extradefs .= ' -D__GNUC__=3';
    $extradefs .= ' -D__GNUC_MINOR__=3';
  }
}

my $result = "$gcc_tweaked$extradefs";
system("rm $tmpfile $outfile 2>/dev/null >/dev/null");
print "$result\n";

sub verbose {
    print STDERR "@_" if $verbose;
}

sub uniquify {
    my @in = @_;
    my %saw;
    return grep(!$saw{$_}++, @in);
}  

my $check_change_done = 0;
sub check_change() { # bug1409: pre-3 versions of gcc incorrectly handle -UVAR -DVAR=val
  if (!$check_change_done) {
    verbose("verifying change works...\n");
    my $out = `$gcc_tweaked -dM - -E -USOME_TEST_DEF -DSOME_TEST_DEF=4 2>/dev/null < /dev/null`;
    if ($out =~ m/SOME_TEST_DEF\s+4/) {
      $check_change_done = 1;
    } else {
      die "ERROR: '$gcc' appears to be too old. This platform requires a working gcc preprocessor which is v3.0 or newer.\n";
    }
  }
}
