#!/usr/bin/env perl

require 5.005;
use strict;
use Getopt::Long;
use IO::File;
use File::Basename;
use File::Find;
use Cwd;

# =======================================================================
# Script that attempts to automate a nightly checkout and build of
# the UPC runtime system.
# It also compiles and runs a collection of test programs
# to check for regressions in the UPC translator.
# =======================================================================

# =======================================================================
# Where to build the runtime system and what to test
# =======================================================================

# =================  CONSTANTS  =======================

# used to parse the system configuration file
# my $TOK_ERROR = 0;
my $TOK_VALUE = 1;
my $TOK_NAMEVAL = 2;
my $TOK_ARRAYBEGIN = 3;
my $TOK_ARRAYEND   = 4;
my $TOK_HASHBEGIN = 5;
my $TOK_HASHEND = 6;
my $TOK_ASSIGN = 7;
my @TOK_NAMES = qw(ERROR VALUE NAMEVAL ARRAY_BEGIN ARRAY_END
		   HASH_BEGIN HASH_END ASSIGN);

my @REQUIRED_SYSCONFIG_FIELDS = qw(network batch_sys queues);

my @VALID_TESTSUITE_KEYWORDS = qw(TestName Flags MakeFlags Files DynamicThreads
				  StaticThreads CompileResult PassExpr
				  FailExpr ExitCode TimeLimit BuildCmd RunCmd RunCmdArgs
			          RequireFeature ProhibitFeature
				  AppArgs AppEnv KnownFailure SaveOutput
                                  BenchmarkResult);
my @VALID_FAILURE_MODES = qw(all 
                             compile-all compile-warning compile-failure
                             run-all run-match run-crash run-time run-mem run-exit run-limit);
my @VALID_COMPILER_RESULTS = qw(pass fail);
my @VALID_BUILD_COMMANDS = qw(make upcc);
my @VALID_FILTERS = (qw(runnable linkable benchmark dynamic static
                        known-run-any known-compile-any known-any),
                     map {'known-'.$_;} @VALID_FAILURE_MODES);

# array that will hold the relative pathnames of all the
# testsuites starting from the source tree top directory
my @ALL_TESTSUITES = ();

# runjobs.pl exit codes
# my $FINISHED_CODE = 0;
my $AGAIN_CODE = 1;
# my $FAILURE_CODE = 2;

# runjobs.pl 'slack'
my $slack = 15;

my %SYS_ENV = ();
my $DASHLINE = "="x78 . "\n";
# prepend the default options set in the environment (if any)
if ($ENV{HARNESS_FLAGS}) {
  my @env_args = split(' ',$ENV{HARNESS_FLAGS});
  unshift @ARGV, @env_args;
}
my $cmdargs = join(' ', @ARGV);

my $warning_blacklist = '^'.
 '((.*?Enabling .experimental. UPC translator optimizations.*?)|'.
  '(.*?warning: overriding commands for target \`Makefile\'.*?)|'.
  '(.*?warning: ignoring old commands for target \`Makefile\'.*?)|'.
  '(.*?make.*?has modification time.*? in the future.*?)|'.
  '(.*?make.*?Clock skew detected.*?)|'.
  '(.*?One or more input object files contain IPA information.*?)|'.
  '(.*?warning: inlining failed in call to.*?)|'.
  '(.*?warning: called from here.*?)|'.
  '(.*? -diag_error .*?)|'.
  '(.*?Warning: -xarch=native has been explicitly specified.*?)|'.
  '(.*?\(E\) Error in message set [0-9]+, unable to retrieve message [0-9]+.*?)|'.
  '(.*?libm.\S+ is not used for resolving any symbol.*?)|'.
  '(.*?In function .*?(\n.*?warning: .*? misused, please use .*?)+)|'.
  '(.*?In function .*?(\n.*?warning: passing argument .*? of \'(_gasnet_.*?|fh_request_free)\' discards qualifiers.*?)+)|'.
  '((ld: 0711-224 WARNING: Duplicate symbol: (__fe_def_env|_?gasnet[it]_).*?\n)+ld: 0711-345.*?)|' .
  '(ld.*?: WARNING 105: Common symbol "_?gasnet[it]_.*?)|' .
  '())$';

my $mydir = $0;
$mydir =~ s@/[^/]*$@@;
push @INC, $mydir;  # set up search path for our perl includes
require "harness_util.pl";

# =================  DEFAULT VALUES  =======================
my $clean_build = 0;        # do we remove testsuite from build tree?
my $keep_binary = 2;        # do we keep the app binary after running it? (2=autodetect)
my @compiler_features = undef;
my $gasnet_config = undef;  # GASNET_CONFIG_STRING, iff we are using upcr and gasnet
my $upcr_config = undef;    # UPCR_CONFIG_STRING, iff we are using upcr and gasnet
my $max_nodes_to_run = 1024;  # dont submit the job if it requires more nodes 
my $default_runtime = 180;  #seconds
my $timeout_multiplier = 1.0;
my $debug = 0;
my $upcc_debug = 0;
my $upcc_tv = 0;            # use totalview -tv flag
my $upcc_profile = 0;       # use --profile flag
my $upcc_profile_local = 0; # use --profile-local flag
my %runlist_num = ();
my $job_number = 0;
my $sysconf = undef;
my $logf = undef;
my $recompile = undef;
my $optimize = 0;
my $trans_opt = 0;
my $runjobs = undef;
my $compileonly = undef;
my @run_lists = ();
my $no_queue_list = undef;
my $nthread_default = 2;    # number of UPC threads
my $num_pthreads = 0;       # default: don't use pthreads
my $repo = undef;         # which account/repository to charge
my $max_proc_per_node = 1;
my $test_file_pat = undef;
my $min_num_nodes = 1;
my $timelimit = 10*60; # compilation time limit (in sec)
my $compiler_spec_file = $ENV{'COMPILER_SPEC_FILE'} || "compiler.spec";
my %compiler_spec = (
	upc_home 			=> '',
	upc_compiler 			=> 'nodefault',
	upcrun_command			=> 'nodefault',
	feature_list			=> 'nodefault',
	upc_static_threads_option	=> 'nodefault',
	suite_path			=> 'nodefault',
	upc_trans_option		=> '',
	default_sysconf                 => 'smp-interactive',
	known_failures			=> '',
	gmake 				=> 'make',
	ar 				=> 'ar',
	ranlib 				=> 'ranlib',
	cc 				=> 'cc',
	cflags 				=> '',
	ldflags				=> '',
	libs 				=> '',
	exe_suffix 			=> '',
	host_cc 			=> '',
	host_cflags 			=> '',
	host_ldflags 			=> '',
	host_libs 			=> ''
	);
my $upc_home_override = undef;

# =======================================================================
# Start of main program
# =======================================================================

# find where this script is located
if (! defined($ENV{TOP_SRCDIR})) {
    &fatal("Environment variable TOP_SRCDIR must be defined");
}
my $top_src_path = $ENV{TOP_SRCDIR};
my $harness_src_path = "$top_src_path/harness";
my $top_work_path = undef;
my $harness_bld_path = undef;
my $startdir = getcwd();
$harness_bld_path = $0;
while (readlink($harness_bld_path)) {
    my $link = readlink($harness_bld_path);
    if (substr($link, 0, 1) eq "/") {
        $harness_bld_path = $link;
    } else {
        $harness_bld_path = dirname($harness_bld_path) . "/" . $link;
    }
}
$harness_bld_path = dirname($harness_bld_path);    # from File::Basename
chdir($harness_bld_path) or die "Can't cd to '$harness_bld_path': $!\n";
$harness_bld_path = getcwd(); # use absolute path

chdir('..') or die "Can't cd to '$harness_bld_path/..': $!\n";
$ENV{TOP_BUILDDIR} = getcwd(); # for use in harness.conf files

chdir($startdir) or die "Can't cd to '$startdir': $!\n";

my $logdir = undef;
my ($datestamp, $timestamp) = &gen_timestamp();

&parse_args();

# Set LANG=C in the environment to ensure consistent compiler messages
$ENV{'LANG'} = 'C';

my $make = $compiler_spec{gmake};

# setup global variables
my $run_report_file = "$logdir/run.rpt";
my $compile_report_file = "$logdir/compile.rpt";
my $knownfailure_report_file = "$logdir/knownfailures.rpt"; # all 'KnownFailure' tags in suites we encounter, regardless of pass/fail
my $knownfailure_rpt = undef;
my $compile_rpt = undef;
my $current_run_dir = undef;


my $test_suites = $sysconf->{testsuites};
my $test_filter = $sysconf->{testfilter};
my $includefilter = $sysconf->{includefilter};
my $excludefilter = $sysconf->{excludefilter};
my $upcc = $compiler_spec{upc_compiler};
$upcc .= " -g " if $upcc_debug;
$upcc .= " -tv " if $upcc_tv;
$upcc .= " -profile " if $upcc_profile;
$upcc .= " -profile-local " if $upcc_profile_local;
$upcc .= " -O" if $optimize;
$upcc .= " -opt" if $trans_opt;
my $upcrun = $compiler_spec{upcrun_command};
my $network = $sysconf->{network};
my $runjobs_script = sprintf("%s/runjobs",$harness_bld_path);

my $system_queues = $sysconf->{queues};
my $run_env = $sysconf->{run_env};
my $run_env_default = $sysconf->{run_env_default};
my $endjob_cmd = $sysconf->{endjob_cmd};
my $batchsys = lc($sysconf->{batch_sys});
my $submit_cmd = "";
my $gen_qscript = undef;
my $ll_share_nodes = "not_shared";
if ($batchsys eq "loadleveler") {
    $submit_cmd = $sysconf->{submit_cmd} || "llsubmit";
    $gen_qscript = \&gen_ll_qscript;
    if (lc($sysconf->{sysname}) eq "cheetah") {
	$ll_share_nodes = "shared";
    }
} elsif ($batchsys eq "pbs") {
    $submit_cmd = $sysconf->{submit_cmd} || "qsub";
    $gen_qscript = \&gen_pbs_qscript;
} elsif ($batchsys eq "sge") {
    $submit_cmd = $sysconf->{submit_cmd} || "qsub";
    $gen_qscript = \&gen_sge_qscript;
} elsif ($batchsys eq "rms_allocate") {
    # an alias for "shell", running interactively w/ RMS's allocate
    $sysconf->{batch_sys} = "shell";
    $submit_cmd = $sysconf->{submit_cmd} || "allocate";
    $gen_qscript = \&gen_rms_allocate_qscript;
} elsif ($batchsys eq "lsf") {
    $submit_cmd = $sysconf->{submit_cmd} || "bsub <";
    $gen_qscript = \&gen_lsf_qscript;
} elsif ($batchsys eq "slurm") {
    $submit_cmd = $sysconf->{submit_cmd} || "sbatch";
    $gen_qscript = \&gen_slurm_qscript;
} elsif ($batchsys eq "interactive") {
    # "interactive" is now an alias for "shell" with an empty submit_cmd
    $sysconf->{batch_sys} = "shell";
    $submit_cmd = '';
    $gen_qscript = \&gen_shell_qscript;
} elsif ($batchsys eq "shell") {
    die "batch_sys = shell, but shell_cmd undefined" unless defined($sysconf->{shell_cmd});
    $submit_cmd = $sysconf->{shell_cmd};
    $gen_qscript = \&gen_shell_qscript;
} else {
    &fatal("Unknown batch system [$batchsys]");
}
my $resubmit_cmd = $sysconf->{resubmit_cmd} || $submit_cmd;

if (defined($sysconf->{repository})) {
    $repo = $sysconf->{repository};
}
if (defined($sysconf->{nthread_default})) {
    $nthread_default = $sysconf->{nthread_default};
}
if (defined($sysconf->{num_pthreads})) {
    $num_pthreads = $sysconf->{num_pthreads};
}
if (defined($sysconf->{max_proc_per_node})) {
    $max_proc_per_node = $sysconf->{max_proc_per_node};
}
if (defined($sysconf->{min_num_nodes})) {
    $min_num_nodes = $sysconf->{min_num_nodes};
}
if (defined($sysconf->{make})) {
    $make = $sysconf->{make};
}
if (defined($sysconf->{max_nodes_to_run})) {
    $max_nodes_to_run = $sysconf->{max_nodes_to_run};
}

# build the top level dirs, if they do not already exist
&mk_dir($logdir);
&start_log();
unlink($run_report_file);
	   
# Set useful upcc variables that might be needed by harness expansions 
$ENV{"AR"} = $compiler_spec{ar}; 
$ENV{"RANLIB"} = $compiler_spec{ranlib}; 
if (have_feature('upcr')) {
  my $upcc_with_flags = "$upcc -network=$network $sysconf->{Flags} "
                        . ($num_pthreads>0?"-pthreads":"-nopthreads");
  my %makevars = (
                'GASNET_CC' 		=> '', 
                'GASNET_OPT_CFLAGS' 	=> '', 
                'GASNET_MISC_CFLAGS'	=> '', 
                'GASNET_MISC_CPPFLAGS'	=> '',
		'GASNET_LDFLAGS'	=> '',
		'LIBS'			=> ''
               );
  foreach my $makevar (keys %makevars) {
    my $cmd = "$upcc_with_flags -echo-var=$makevar";
    my $val = `$cmd`;
    chomp($val);
    print "Querying upcc: '$cmd' => $val\n" if $debug; 
    $makevars{"$makevar"} = $val;
  }
  $compiler_spec{cc} = $makevars{"GASNET_CC"} if (!$compiler_spec{cc});
  # omit GASNET_OPT_CFLAGS for now, because it includes aggressive enable-warning options
  $compiler_spec{cflags} = $makevars{"GASNET_MISC_CFLAGS"} . " " .
                   $makevars{"GASNET_MISC_CPPFLAGS"} if (!$compiler_spec{cflags});
  $compiler_spec{ldflags} = ($makevars{"GASNET_LDFLAGS"}||'') if (!$compiler_spec{ldflags});
  $compiler_spec{libs} = ($makevars{"LIBS"}||'') if (!$compiler_spec{libs});

  my $sizes_file = "$logdir/upcc-sizes";
  my $cmd = "$upcc_with_flags -show-sizes > $sizes_file";
  &logit("querying upcc-sizes...");
  system($cmd);
  open (SIZES, $sizes_file) || die "Failed to open $sizes_file: $!\n";
  while (<SIZES>) {
    if (m/^GASNetConfig\s+(.*)$/) {
	$gasnet_config = $1;
    }
    if (m/^UPCRConfig\s+(.*)$/) {
	$upcr_config = $1;
    }
  }
  close SIZES;
  &logit("Build config strings:\nUPCRConfig: $upcr_config\nGASNetConfig: $gasnet_config");
  if ($gasnet_config) {
    foreach my $config (split(',',$gasnet_config)) {
      if ($config =~ m/^atomic/ || $config =~ m/^timers_/ || $config =~ m/^(no)?align$/) {
        push @compiler_features, $config;
      }
    }
  }

  my $version_file = "$logdir/upcc-version";
  my $cmd = "$upcc_with_flags -version > $version_file";
  &logit("querying upcc -version...");
  system($cmd);
  open (VERSION, $version_file) || die "Failed to open $version_file: $!\n";
  my $oldslash = $/;
  undef $/;
  my $version_str = <VERSION>; # slurp!
  $/ = $oldslash;
  close VERSION;
  &logit("upcc -version:\n$version_str");

  if ($version_str =~ m/UPC-to-C translator(.*?)-\+-/s) {
    my $trans_ver = $1;
    if ($trans_ver =~ m/host\s+\S+\s+([^-]+)-([^\/]+)\/(\d+)/) {
        push @compiler_features, "trans_os_$1";
        push @compiler_features, "trans_cpu_$2";
        push @compiler_features, "trans_cpu_$3";
    }
  }
}
if (!$compiler_spec{host_cc}) {
  $compiler_spec{host_cc} = $compiler_spec{cc};
  $compiler_spec{host_cflags} = $compiler_spec{cflags};
  $compiler_spec{host_ldflags} = $compiler_spec{ldflags};
  $compiler_spec{host_libs} = $compiler_spec{libs};
}
die "missing compiler spec setting 'cc'" if (!$compiler_spec{cc});
my $reportstr = "C compiler settings:";
foreach my $n ("cc","cflags","ldflags","libs",
         "host_cc","host_cflags","host_ldflags","host_libs") {
  $ENV{uc($n)} = $compiler_spec{$n}; 
  $reportstr .= "\n" . uc($n) . "=" . $compiler_spec{$n}; 
}
&logit($reportstr);

if ($recompile) {
    # open the compile report file
    $compile_rpt = new IO::File("> $compile_report_file");
    if (! defined($compile_rpt)) {
	&fatal("Could not open file [$compile_report_file]");
    }
}

my $suitepath;
if ($clean_build) {
  # clean everything before copying/running any suites to avoid clobbering 
  # suites which might be nested
  foreach $suitepath (@$test_suites) {
    &clean_suite($suitepath);
  }
}

foreach $suitepath (@$test_suites) {
    &copy_suite($suitepath);
}

# open the knownfailure report file
$knownfailure_rpt = new IO::File("> $knownfailure_report_file");
if (! defined($knownfailure_rpt)) {
   &fatal("Could not open file [$knownfailure_report_file]");
}
foreach $suitepath (@$test_suites) {
    &run_suite($suitepath);
}
undef $knownfailure_rpt;

if ($recompile) {
    printf $compile_rpt $DASHLINE;
    printf $compile_rpt "Compilation COMPLETE\n";
    my $compile_done = new IO::File("> $logdir/compile-complete");
    if (defined($compile_done)) {
        my ($datestamp, $timestamp) = &gen_timestamp();
        printf $compile_done "$timestamp\n";
        undef $compile_done;
    }
}

&close_all_runlists();
&logit("Run scripts and reports can be found in $logdir");
&logit("Harness compilation completed.");

# close remaining open file handles
$compile_rpt = undef if (defined($compile_rpt));
$logf = undef if (defined($logf));

exit 0;

# =======================================================================
# find the testsuites
# =======================================================================
sub find_testsuites {
    print "Gathering test suite information...\n";
    @ALL_TESTSUITES = ();
    my $suite_path = $compiler_spec{suite_path};
    my @dirs = split(':', $suite_path);
    find(\&gather_testsuite, @dirs);
    if (@ALL_TESTSUITES == 0) {
	&fatal("Could not find testsuites starting from [$suite_path]");
    }
}

# =======================================================================
# function to identify the testsuites by recersive find
# =======================================================================
sub gather_testsuite {
    return if ($_ ne "harness.conf");
    my $dir = $File::Find::dir;
    if (1) {
      push(@ALL_TESTSUITES,$dir);
    } else {
      foreach my $suite_dir (split(':', $compiler_spec{suite_path})) {
        if ($dir  =~ /$suite_dir\/(.+)$/) {
	  push(@ALL_TESTSUITES,$1);
        }
      }
    }
}

# =======================================================================
# Change to a directory
# =======================================================================
sub cd {
    my $dir = shift;
    if (!chdir($dir)) {
	&fatal("Could not cd to [$dir]");
    }
}

# =======================================================================
# Create a directory if it does not already exist
# =======================================================================
sub mk_dir {
    my $dir = shift;
    return if (-e $dir);
    my $parent = dirname($dir);
    if (! -e $parent) {
	&mk_dir($parent);
    }
    if (!mkdir($dir, 0755)) {
	&fatal("Could not create directory [$dir]");
    }
}
    
# =======================================================================
# Create the date and time stamp for this run
# =======================================================================
sub gen_timestamp {
    my ($sec,$min,$hour,$day,$mon,$year) = (localtime(time))[0..5];
    $year += 1900;
    $mon++;
    my $timestamp = sprintf("%04d%02d%02d_%02d%02d%02d",
			 $year,$mon,$day,$hour,$min,$sec);
    my $datestamp = sprintf("%02d/%02d/%04d at %02d:%02d:%02d",
			 $mon,$day,$year,$hour,$min,$sec);
    return ($datestamp, $timestamp);
}

# =======================================================================
# Create the log file
# =======================================================================
sub start_log {
    &fatal("Timestamp not defined") if (!defined($timestamp));
    my $logfile = "$logdir/log";
    $logf = new IO::File("> $logfile");
    &fatal("Cannot create file [$logfile]") if (!defined($logf));
    &logit("Args: $cmdargs");
    &logit("Started: $datestamp");
}

# ===============================================================
# Write message to log file and to stdout
# ===============================================================
sub logit {
    my $str = shift;
    my($sec,$min,$hour) = (localtime(time))[0,1,2];
    if (defined($logf)) {
	printf $logf $DASHLINE;
	printf $logf ("%02d:%02d:%02d: %s\n",$hour,$min,$sec,$str);
    }
    printf $DASHLINE;
    printf ("%02d:%02d:%02d: %s\n",$hour,$min,$sec,$str);
}

# ===============================================================
# Oops, crash and burn
# ===============================================================
sub fatal {
    my $str = shift;
    &logit("FATAL: $str");
    exit 1;
}

# ===============================================================
# Oops, complain loudly
# ===============================================================
sub error {
    my $str = shift;
    &logit("HARNESSERROR: $str");
}

# ===============================================================
# Oops, complain and invalidate current entry
# ===============================================================
sub syntax {
    my $str = shift;
    my $spec = shift;

    if (defined($spec)) {
	$spec->{SyntaxError} = 1;	# Better/easier than deleting it
	&error("$str - ignoring input until next TestName.");
    } else {
	&fatal($str);
    }
}

# ======================================================================
# Find suite paths for a given suite
# ======================================================================
sub get_suite_srcbld_paths {
    my $src_suite = shift;
    foreach my $suite_dir (split(':', $compiler_spec{suite_path})) {
      if (-f "$suite_dir/$src_suite/harness.conf") {
	$src_suite = "$suite_dir/$src_suite"; last;
      }
    }
    my $bld_suite = $src_suite;
    $bld_suite =~ s/$top_src_path/$top_work_path/;
    return ($src_suite,$bld_suite);
}
# ======================================================================
# Clean suite dirs 
# ======================================================================
sub clean_suite {
    my ($src_suite,$bld_suite) = get_suite_srcbld_paths(shift);
    # check that the source tree exists
    if (! -f "$src_suite/harness.conf") {
	&logit("WARNING: Source Tree Suite ${src_suite} does not exist.  Skipping...");
	return;
    }
    if (($src_suite ne $bld_suite) && (-d $bld_suite)) { 
        &logit("Removing Testsuite [$bld_suite]");                                                 
        my $out = `rm -r \'$bld_suite\'`;                                                          
        my $status = $?;                                                                           
        if ($status != 0) {                                                                        
            &fatal("Failed to remove $bld_suite, status = [$status] out=[$out]");                  
        }                                                                                          
    }
}

# ======================================================================
# Copy suite dirs
# ======================================================================
sub copy_suite {
    my ($src_suite,$bld_suite) = get_suite_srcbld_paths(shift);
    # check that the source tree exists
    if (! -f "$src_suite/harness.conf") {
	&logit("WARNING: Source Tree Suite ${src_suite} does not exist.  Skipping...");
	return;
    }
    # copy the source suite to the build suite, if they are not the
    # same paths.
    if ($src_suite ne $bld_suite) {
	if (! -f "$bld_suite/harness.conf") {
	    # copy the source directory structure to the build tree
	    &logit("Copying Testsuite [$src_suite] to [$bld_suite]");
	    my $parent = dirname($bld_suite);
	    my $out = `mkdir -p $parent`;
	    $out = `cp -rp \'$src_suite\' \'$parent\'`;
	    my $status = $?;
	    if ($status != 0) {
		&fatal("Failed to copy $src_suite to $parent, status = [$status] out=[$out]");
	    }
	    # hack for the NPB suite
	    my $parent_base = basename($parent);
	    my $src_parent = dirname($src_suite);
	    if ($parent_base =~ /^NPB/) {
		if (! -d "$parent/common") {
		    `cp -rp ${src_parent}/common $parent`;
		    `cp -rp ${src_parent}/config $parent`;
		    `cp -rp ${src_parent}/sys $parent`;
		}
	    }
	}
    }
}

# ======================================================================
# Process the test suite.
# Compile the sources (if requested)
# Generate the runlist and (if necessary) the queue script to run the jobs
# ======================================================================
sub run_suite {
    my ($src_suite,$bld_suite) = get_suite_srcbld_paths(shift);

    my $suite = basename($bld_suite);

    $current_run_dir = $bld_suite;
    &logit("Entering test suite $suite [${current_run_dir}]");
    &cd($current_run_dir);

    my $h_conf = "harness.conf";
    
    my $conf_arr = &read_suite_config($suite,$h_conf);

    &logit("Using feature list: [".join(',',@compiler_features)."]");

    TEST:
    while (my $conf = shift(@$conf_arr)) {
	my $runit = 0;
	my $code = $conf->{TestName};
	if (defined($conf->{SyntaxError})) {
	    &error("Skipping app $suite/$code, because an error was detected in its configuration");
	    next;
	}
	if (defined($test_file_pat) && ($code !~ /$test_file_pat/)) {
	    &logit("Skipping app $suite/$code, no match with [$test_file_pat]");
	    next;
	}
	my $barecode = $code;
        $barecode =~ s/_st\d+$//;
	if (defined(@$test_filter) && !grep(/^$barecode$/i, @$test_filter) && !grep(/^$suite\/$barecode$/i, @$test_filter)) {
	    &logit("Skipping app $suite/$code, no match with [".join(',',@$test_filter)."]");
	    next;
	}
	if (defined(@$includefilter)) {
	    foreach my $filter (@$includefilter) {
	        if (!check_filter($filter, $conf)) {
	            &logit("Skipping app $suite/$code, no match with -include=$filter");
	            next TEST;
	        }
	    }
	}
	if (defined(@$excludefilter)) {
	    foreach my $filter (@$excludefilter) {
	        if (check_filter($filter, $conf)) {
	            &logit("Skipping app $suite/$code, match with -exclude=$filter");
	            next TEST;
	        }
	    }
	}
    	if (defined($conf->{RequireFeature})) {
	    foreach my $feature (split(/[, ]/,$conf->{RequireFeature})) {
	      $feature =~ s/\s+$//g;
	      $feature =~ s/^\s+//g;
	      if (!grep(/^$feature$/i,@compiler_features)) {
                &logit("Skipping test [$suite/$code] because it requires missing feature [$feature]");
	        next TEST;
	      }
	    }
        }
    	if (defined($conf->{ProhibitFeature})) {
	    foreach my $feature (split(/[, ]/,$conf->{ProhibitFeature})) {
	      $feature =~ s/\s+$//g;
	      $feature =~ s/^\s+//g;
	      if (grep(/^$feature$/i,@compiler_features)) {
                &logit("Skipping test [$suite/$code] because it prohibits feature [$feature]");
	        next TEST;
	      }
	    }
    	}
	if ($recompile) {
	    $runit = &compile_test($suite,$conf);
	} else {
	    $runit = (-x $code);
	}

	if ($compileonly) {
	    &logit("CompileOnly, removing $suite/$code");
	    &remove_binary($conf);
	    next;
	}
	if (defined($conf->{NoLink})) {
	    &logit("NoLink, will not submit $suite/$code");
	    next;
	}
	if (defined($conf->{TimeLimit}) && $conf->{TimeLimit} == 0) {
	    &logit("Will not submit $suite/$code, TimeLimit == 0");
	    next;
	}
	
	if ($runit) {
	    &logit("Submitting application $code");
	    if (defined($conf->{StaticThreads})) {
		my $nth = $conf->{StaticThreads};
		&submit_test($suite,$conf,$nth) if ($nth > 0);
	    }
	    if (defined($conf->{DynamicThreads}) &&
		(my $arr = &extract_array($conf->{DynamicThreads}))) {
		my @nthread = grep(!/^0$/,@$arr);
		my $tmpkeep = $keep_binary;
                # bug 1266: force keep for tests with 2 or more dynamic thread counts
		$keep_binary = 1 if (scalar(@nthread) >= 2); 
		foreach my $nth (@nthread) {
		    &submit_test($suite,$conf,$nth);
		}
		$keep_binary = $tmpkeep;
	    }
	} else {
	    &logit("Will not run application $code");
	}
    }
}

# ======================================================================
# Close out all opened qscripts and submit to batch system if requested
# ======================================================================
sub close_all_runlists {
    my $rlist;

    while ($rlist = shift(@run_lists)) {
	&close_runlist($rlist);
    }
    if (defined($no_queue_list)) {
	undef $no_queue_list->{fh};
	my $name = $no_queue_list->{listname};
	my $numjobs = $no_queue_list->{numjobs};
	&logit("Closing $name with $numjobs jobs, will not run");
    }
}

# ======================================================================
# Close the qscript and submit the job to the batch system (if requested).
# ======================================================================
sub close_runlist {
    my $rlist = shift;

    # close the runlist file handle
    $rlist->{fh} = undef;

    my $name = $rlist->{listname};
    my $numjobs = $rlist->{numjobs};
    my $nproc = $rlist->{nproc};
    my $num_nodes = $rlist->{nodes};
    &logit("Closing runlist $name with $nproc processes and $numjobs jobs");
    if ($runjobs && ($numjobs > 0)) {
	my $qpath = $rlist->{qpath};
	my $submit = $rlist->{submit_cmd} || $submit_cmd;
	my $script = basename($qpath);
	if ($num_nodes > $max_nodes_to_run) {
	    &logit("Will not start job $script, requires $num_nodes > limit of $max_nodes_to_run nodes");
	    return;
	}
	if (lc($sysconf->{batch_sys}) eq "shell") {
            my $shell_cmd = $submit ? "'$submit $script'" : "job $script";
            &logit("Starting interactive run of $shell_cmd");
	    my $out = `$submit $qpath`;
	    my $status = $?;
	} else {
	    my $out = `$submit $qpath 2>&1`;
	    my $status = $?;
	    if ($status == 0) {
	        &logit("Job $script submitted to batch system:\n$submit $qpath\n$out");
	    } else {
	        &error("Job submission of [$qpath] failed with exit code $status:\n$submit $qpath\n$out");
	    }
        }
    }
}

# ======================================================================
# Parse known failures spec
# ======================================================================
sub trim_ends {
  my $val = shift;
  $val =~ s/^\s+//g;
  $val =~ s/\s+$//g;
  return $val;
}
sub parse_known {
    my $str = shift;
    my $loc = shift;
    my $curspec = shift;

    my $modestr = undef; 
    my $featurestr = undef;
    my $descstr = undef;
    if ($str =~ /^\s*([A-Za-z0-9_-]+(?:\s*,\s*[A-Za-z0-9_-]+)*)?\s*;(.*?);\s*(.*?)$/) {
       $modestr = $1 if ($1);
       $featurestr = $2 if ($2);
       $descstr = $3 if ($3);
    } else {
       $descstr = $str;
    }
    $modestr = trim_ends($modestr);
    $modestr = "all" if (!$modestr);
    $descstr = trim_ends($descstr);
    $descstr = '(No comment given)' if (!$descstr);
    $descstr =~ tr/';//d;
    $featurestr = trim_ends($featurestr);
    $featurestr = "all" if (!$featurestr);
    my $featureexpr = " $featurestr ";
    $featureexpr =~ s!([^a-zA-Z_\&'-])([a-zA-Z0-9_-]+)!$1(\&have_feature('\L$2\E'))!g;

    my @modes = split(',',$modestr);
    map { s/^\s*(\S+)\s*$/\L$1\E/; } @modes;
    foreach my $mode (@modes) {
          if (! grep(/^$mode$/,@VALID_FAILURE_MODES)) {
              &syntax("Unknown failure mode [$mode] at $loc\n".
                      "Valid modes are: ".join(',',@VALID_FAILURE_MODES), $curspec);
          }
    }
    $modestr = join(',',@modes);
    print "parse_known: modestr: $modestr featureexpr: $featureexpr descstr: $descstr\n" if ($debug);
    $curspec->{KnownFailure} = () if (!defined($curspec->{KnownFailure}));
    $curspec->{KnownFailure}[scalar @{$curspec->{KnownFailure}}] = [",$modestr,", $featureexpr, $descstr];
    print $knownfailure_rpt $curspec->{SuiteName}."/".$curspec->{TestName} . 
         ": ".feature_applicable($featureexpr)." ; $modestr ; $featurestr ; $descstr\n";
}

sub parse_compilerspec_failure {
  my $suite = shift;
  my $test = shift;
  my $loc = shift;
  my $curspec = shift;
  if ($compiler_spec{known_failures}) {
     my $knownstr = $compiler_spec{known_failures};
     # special case to allow || in featurestr
     $knownstr =~ s/([^\|\\])\|([^\|])/$1 \| $2/g; # ensure all lone pipes are surrounded with spaces
     my @knownfails = split(/ \| /,$knownstr);
     my @match = grep(/^$suite\/$test([\[\s].*)?$/,@knownfails);
     if (!@match) {
        push @match, grep(/^$test([\[\s].*)?$/,@knownfails);
     }
     if (@match) { 
       foreach my $m (@match) {
         my $matchstr = "(No comment given)";
         if ($m =~ /[^[]+\[(.*)\]/) {
           $matchstr = $1;
         }
	 $matchstr =~ s/\\([\[\]\|])/$1/g;
         parse_known($matchstr, $loc, $curspec);
       }
     }
  }
}

sub feature_applicable {
  my $_featureexpr = shift;
  my $_result;
  eval "\$_result = ($_featureexpr)";
  die "error in eval of '$_featureexpr': $@\n" if ($@);
  return $_result;
}

sub get_runknown_str {
  my $conf = shift;
  my $str = '';
  foreach my $failtype (@{$conf->{KnownFailure}}) {
    my ($modes, $featurestr, $descstr) = @{$failtype};
    if ($modes =~ /,run-/ || $modes =~ /,all,/) {
      print "get_runknown_str: modes: $modes featurestr: $featurestr descstr: $descstr\n" if ($debug);
      if (feature_applicable($featurestr)) {
        $modes = join(',',grep(/^run-/,split(',',$modes)));
        $modes = 'all' if (!$modes);
        $str .= "|$modes;$descstr";
      }
    }
  }
  $str =~ s/^\|//;
  print "get_runknown_str: $str\n" if ($debug);
  return "$str";
}

# ======================================================================
# Flag known failures
# ======================================================================
sub have_feature {
  my $name = shift;
  return grep(/^$name$/,@compiler_features);
} 

sub check_known {
    my $str = shift;
    my $conf = shift;
    my $failure_mode = shift;
    my $knowndesc = undef;

    if (defined($conf->{KnownFailure})) {
      foreach my $failtype (@{$conf->{KnownFailure}}) {
        my ($modes, $featurestr, $descstr) = @{$failtype};
        print "checking '$failure_mode' against KnownFailure: $modes ; $featurestr ; $descstr\n" if ($debug);
        if ($modes =~ /,$failure_mode,/ || $modes =~ /,all,/ || $modes =~ /,compile-all,/ ) {
          if (feature_applicable($featurestr)) {
            print "matched with failure mode '$failure_mode'\n" if ($debug);
            $knowndesc = $descstr;
            last;
          } else {
            print "failure mode '$failure_mode' matched, but featurestr did not\n" if ($debug);
          }
        }
      }
    }

    chomp $str;
    if (defined($knowndesc)) {
	$str .=   "\t(KNOWN)\n" .
		   "Known failure: $knowndesc\n";
    } else {
	$str .=   "\t(NEW)\n";
    }
    return $str;
}

sub check_filter {
    my $filter = shift;
    my $conf = shift;

    if ($filter eq "runnable") {
      return !defined($conf->{NoLink}) &&
	     !(defined($conf->{TimeLimit}) && $conf->{TimeLimit} == 0);
    }
    if ($filter eq "linkable") {
      return !defined($conf->{NoLink});
    }
    if ($filter eq "benchmark") {
      return defined($conf->{BenchmarkResult})
    }
    if ($filter eq "static") {
      return defined($conf->{StaticThreads})
    }
    if ($filter eq "dynamic") {
      return defined($conf->{DynamicThreads})
    }
    if (($filter =~ m/^require-feature:(.+)/) && defined($conf->{RequireFeature})) {
        my $pattern = "\s*$1\s*";
        return grep(/^$pattern$/i,split(/[, ]/,$conf->{RequireFeature}));
    }
    if (($filter =~ m/^prohibit-feature:(.+)/) && defined($conf->{ProhibitFeature})) {
        my $pattern = "\s*$1\s*";
        return grep(/^$pattern$/i,split(/[, ]/,$conf->{ProhibitFeature}));
    }
    if (($filter =~ m/^known-([a-z-]+?)(\+?)$/) && defined($conf->{KnownFailure})) {
      my ($pattern, $nofeature) = ($1, $2);
      if ($pattern eq "any") {
         $pattern = '[a-z-]+';
      } elsif ($pattern eq "run-any") {
         $pattern = '(all|run-[a-z]+)';
      } elsif ($pattern eq "compile-any") {
         $pattern = '(all|compile-[a-z]+)';
      }
      foreach my $failtype (@{$conf->{KnownFailure}}) {
        my ($modes, $featurestr, $descstr) = @{$failtype};
        return 1 if (($modes =~ m/,$pattern,/) &&
		     ($nofeature || feature_applicable($featurestr)));
      }
    }

    # Note that the case of an invalid filter was checked when command line flags were parsed
    return 0;
}


# ======================================================================
# convert a string of the form HHH:MM:SS to seconds
# ======================================================================
sub time_in_sec {
    my $str = shift;

    if ($str =~ /^(\d+):(\d+):(\d+)$/) {
	return 3600*$1 + 60*$2 + $3;
    } else {
	&fatal("time_in_sec: invalid input [$str]");
    }
}

# ======================================================================
# convert a time in seconds to a string of the form HHH:MM:SS
# ======================================================================
sub secs_to_HMS {
   my $secs = shift;
   my $hrs = int($secs / 3600);
   $secs -= 3600 * $hrs;
   my $mins = int($secs / 60);
   $secs -= 60 * $mins;
   return sprintf("%d:%02d:%02d", $hrs, $mins, $secs);
}

# ======================================================================
# convert a time in seconds to a string of the form HHH:MM
# ======================================================================
sub secs_to_HM {
   my $secs = shift;
   my $hrs = int($secs / 3600);
   $secs -= 3600 * $hrs;
   my $mins = int($secs / 60);
   return sprintf("%d:%02d", $hrs, $mins);
}

# ======================================================================
# Find and return a runlist associated with a batch queue 
# from which we can launch a job requiring $nthread THREADS
# and that will run for at most $timelimit seconds.
#
# First, search existing lists.  If one is found return it.
#
# If a existing script cannot be found, search the list of queues
# for one that can satisfy the request.  If found, create a new
# runlist for that queue and return it.
# ======================================================================
sub select_runlist {
    my $nproc = shift;
    my $runtime = shift;

    # search existing list of lists for one that will satisfy
    # the requirements
    my $rlist;
    foreach $rlist (@run_lists) {
	my $numproc = $rlist->{nproc};
	my $timelimit = $rlist->{timelimit};

	if ( ($nproc == $numproc) && ((($runtime + $slack) < $timelimit) || ($timelimit == 0))) {
	    # If adding one more entry would exceed threshold, then close the runlist now.
	    # The check for non-zero $totaltime prevents closing an empty runlist
	    my $totaltime = $rlist->{totaltime};
	    my $threshold = $rlist->{submit_threshold};
	    if ($threshold && $totaltime && (($totaltime + $runtime) >= $threshold)) {
	        &close_runlist($rlist);
		# Remove the now submitted runlist and continue as if no match found
	        @run_lists = grep($_ != $rlist, @run_lists);
		last;
	    }

	    return $rlist;
	}
    }

    # if we got here, we have to start a new list.
    # Select a queue that will satisfy the request.

    my $slop = 20;
    my $q;
    foreach $q (@$system_queues) {
	my $qtime_sec = &time_in_sec($q->{Q_maxtime});
	next if (($runtime + $slop) > $qtime_sec && $qtime_sec != 0);

	# max number of tasks per node allowed by the queue
	my $maxtpn = $q->{Q_maxtpn};

	$maxtpn = ($max_proc_per_node < $maxtpn ? $max_proc_per_node : $maxtpn);

	# how many nodes of the cluster will we need?
	my $nodes = int($nproc/$maxtpn);
	$nodes++ if ($maxtpn*$nodes < $nproc);

	# are we inforcing a minimum number of nodes?
	$nodes = $min_num_nodes if ($nodes < $min_num_nodes);

	# violate the minimum if not enough threads
	$nodes = $nproc if ($nodes > $nproc);

	# compute the number of threads to use per node
	my $tpn = int($nproc/$nodes);
	$tpn++ if ($nodes * $tpn < $nproc);

	# this should never happen, but check it anyway
	&fatal("select_runlist: tpn = $tpn, nodes = $nodes, nproc = $nproc, maxtpn = $maxtpn") if (($tpn > $maxtpn) || ($tpn <= 0));

	# does this queue satisfy the node limits?
	next if ($nodes > $q->{Q_maxnode});
	next if ($nodes < $q->{Q_minnode});

	# yea, we found a queue.  Now lets start a new
	# script to run this job
	my $rlist = &new_runlist($q,$nodes,$tpn,$nproc,($runtime + $slack + $slop));

	push(@run_lists,$rlist);

	return $rlist;
    }

    # if we got here, life sucks.  There is no queue
    # with appropriate limits to run this job.
    # return undef to inform caller that we failed
    return undef;
    &fatal("Cant find queue to run job with $nproc processes and $runtime time limit");


}
    

# ======================================================================
# Construct and return a new queue script for the given queue
# and number of processes.
# ======================================================================
sub new_runlist {
    my $q = shift;
    my $nodes = shift;
    my $tpn = shift;
    my $nproc = shift;
    my $mintime = shift;

    if (! defined($runlist_num{$nodes})) {
	$runlist_num{$nodes} = 0;
    }
    my $rl_num = $runlist_num{$nodes};
    $runlist_num{$nodes}++;

    my $runlistname = sprintf("runlist_%03d_%d",$nproc,$rl_num);
    my $runlistpath = "$logdir/$runlistname";
    my $max_qtime_sec = &time_in_sec($q->{Q_maxtime});

    my $threshold = 0;
    if ($sysconf->{submit_threshold}) {
    	if (!$max_qtime_sec) {
	    $threshold = $sysconf->{submit_threshold};
    	} elsif ($sysconf->{submit_threshold} < $max_qtime_sec) {
	    $threshold = $sysconf->{submit_threshold};
	    # Shorten qtime UNLESS it would be too short for the current job
	    $max_qtime_sec = (($threshold > $mintime) ?  $threshold : $mintime);
	} else {
	    $threshold = $max_qtime_sec;
	}
    }

    my $fh = new IO::File("> $runlistpath");
    &fatal("Could not create script [$runlistpath]") if !defined($fh);

    my $rlist = {'listname'      => $runlistname,
		 'rl_num'        => $rl_num,
		 'nproc'         => $nproc,
		 'nodes'         => $nodes,
		 'tpn'           => $tpn,
		 'queue'         => $q,
		 'timelimit'     => $max_qtime_sec,
		 'fh'            => $fh,
		 'numjobs'       => 0,
		 'submit_threshold' => $threshold,
		 'totaltime'     => 0
		 };

    &logit("Allocating new runlist [$runlistname] with $nproc processes and $max_qtime_sec seconds");

    # Need to write the qscript, -- system specific
    &${gen_qscript}($rlist);
    
}

sub output_run_env {
  my $bf = shift;

  printf $bf ("# --- BEGIN SYSCONF VARS ---\n");
  foreach my $key (keys %$run_env_default) {
    my $value = ( $ENV{$key} ? $ENV{$key} : $run_env_default->{$key});
    &logit("setting sysconf environment var: $key='$value'");
    printf $bf ("${key}='${value}'\n");
    printf $bf ("export ${key}\n");
  }

  foreach my $key (keys %$run_env) {
    my $value = $run_env->{$key};
    &logit("setting sysconf environment var: $key='$value'");
    printf $bf ("${key}='${value}'\n");
    printf $bf ("export ${key}\n");
  }
  printf $bf ("# --- END SYSCONF VARS ---\n");
}

sub output_endjob_cmds {
  my $bf = shift;

  printf $bf ("# --- BEGIN CLEANUP COMMANDS ---\n");
  foreach my $cmd ($endjob_cmd) {
    &logit("adding cleanup cmd: $cmd");
    printf $bf ("$cmd\n");
  }
  printf $bf ("# --- END CLEANUP COMMANDS ---\n");
}

sub get_jobname {
    my $num = shift;
    my $jobnum = sprintf("%02d",$num);
    my $network = $sysconf->{network};
    $network = substr($network,0,4);
    $network =~ s/^(.)/\U$1\E/;

    my $pthreads = ($sysconf->{num_pthreads}?"Pth":"");

    my $compiler = (grep(/^cc_/i,@compiler_features))[0];
    $compiler =~ s/^cc_//;
    $compiler =~ s/^(.)/\U$1\E/;
    $compiler = substr($compiler,0,3);

    my $debug = (grep(/debug$/i,@compiler_features))[0];
    if ($debug eq "nodebug") {
      $debug = "Opt";
    } else {
      $debug = "Dbg";
    }

    my $str = "${network}${debug}${pthreads}${compiler}${jobnum}";
    $str =~ s/[^A-Za-z0-9_]/_/g;
    return $str;
}

# =======================================================================
# Generate an IBM SP LoadLeveler queue script
# =======================================================================
sub gen_ll_qscript {
    my $rlist = shift;
    
    my $jobnum = $job_number;
    $job_number++;
    my $nproc = $rlist->{nproc};

    my $q = $rlist->{queue};

    my $runlistfile = $rlist->{listname};
    my $jobname = get_jobname($jobnum);
    my $scriptname = sprintf("qscript_%03d",$jobnum);
    my $qpath = sprintf("%s/%s",$logdir,$scriptname);
    my $outpath = sprintf("%s/out_%03d",$logdir,$jobnum);
    my $errpath = sprintf("%s/err_%03d",$logdir,$jobnum);
    my $max_qtime_sec = $rlist->{timelimit};

    $rlist->{qpath} = $qpath;

    my $bf = new IO::File("> $qpath");
    &fatal("Could not create script [$qpath]") if !defined($bf);

    # write the header for an LL job
    printf $bf ("\#!/bin/sh\n");
    printf $bf ("\#@ job_name         = $jobname\n");
    if ($repo && $repo ne "NA") {
	printf $bf ("\#@ account_no       = ${repo}\n");
    }
    printf $bf ("\#@ output           = $outpath\n");
    printf $bf ("\#@ error            = $errpath\n");
    printf $bf ("\#@ job_type         = parallel\n");
    printf $bf ("\#@ environment      = COPY_ALL\n");
    # dont gen a network specification if using SMP conduit
    if (lc($network) !~ /smp/) {
	my $uc_network = uc($network);
	my $rhs = ($sysconf->{"ll_network_$uc_network"} ||
			('csss,' . $ll_share_nodes . ',us'));
	printf $bf ("\#@ network.%s       = %s\n",
			$uc_network, $rhs);
    }
    printf $bf ("\#@ node_usage       = %s\n",$ll_share_nodes);
    printf $bf ("\#@ class            = %s\n",$q->{Q_name});
    printf $bf ("\#@ node             = %d\n",$rlist->{nodes});
    printf $bf ("\#@ total_tasks      = %d\n",$nproc);
    printf $bf ("\#@ wall_clock_limit = %s\n",&secs_to_HMS($max_qtime_sec));
    printf $bf ("\#@ queue\n");

    printf $bf ("\n");
    printf $bf ("QSCRIPT=$qpath\n");
    printf $bf ("export QSCRIPT\n");
    printf $bf ("RUNJOBS=${runjobs_script}\n");
    printf $bf ("export RUNJOBS\n");
    my $runlog = sprintf("%s/run_%03d.log",$logdir,$jobnum);
    unlink($runlog);
    printf $bf ("RUNLOG=$runlog\n");
    printf $bf ("export RUNLOG\n");
    printf $bf ("TIMELIM=${max_qtime_sec}\n");
    printf $bf ("export TIMELIM\n");
    printf $bf ("\n");

    output_run_env($bf);

    printf $bf ("echo Starting new batch run >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    printf $bf ("echo \"NODES\" >> \$RUNLOG\n");
    printf $bf ("echo \$LOADL_PROCESSOR_LIST >> \$RUNLOG\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("\n");

    my $rpt = basename($run_report_file);
    printf $bf ("\$RUNJOBS $logdir ${runlistfile} $rpt \'\' \$TIMELIM $slack >> \$RUNLOG 2>&1\n");
    printf $bf ("status=\$\?\n");
    printf $bf ("echo STATUS = \$status >> \$RUNLOG\n");
    printf $bf ("if \[ \$status -eq ${AGAIN_CODE} \] ; then\n");
    printf $bf ("   echo Will resubmit job [\$QSCRIPT] >> \$RUNLOG\n");
    printf $bf ("   $resubmit_cmd \$QSCRIPT >> \$RUNLOG 2>&1\n");
    printf $bf ("fi\n\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    output_endjob_cmds($bf);
    undef $bf;

    return $rlist;
}
    
# =======================================================================
# Generate a PBS queue script
# =======================================================================
sub gen_pbs_qscript {
    my $rlist = shift;
    
    my $jobnum = $job_number;
    $job_number++;

    my $q = $rlist->{queue};

    my $runlistfile = $rlist->{listname};
    my $jobname = get_jobname($jobnum);
    $jobname = substr($jobname,0,15); # PBS -N 15 char max
    my $scriptname = sprintf("qscript_%03d",$jobnum);
    my $qpath = sprintf("%s/%s",$logdir,$scriptname);
    my $outpath = sprintf("%s/out_%03d",$logdir,$jobnum);
    my $errpath = sprintf("%s/err_%03d",$logdir,$jobnum);
    my $max_qtime_sec = $rlist->{timelimit};

    $rlist->{qpath} = $qpath;

    my $bf = new IO::File("> $qpath");
    &fatal("Could not create script [$qpath]") if !defined($bf);
    chmod +(0777 & ~umask), $qpath;

    # write the header for an PBS job
    printf $bf ("#!/bin/sh\n");
    printf $bf ("#PBS -N $jobname\n");
    if ($repo && $repo ne "NA") {
      printf $bf ("#PBS -A $repo\n");
    }
    my $reslist = "";
    if ($q->{Q_usesize}) {
      $reslist .= "size=".$rlist->{nodes};
    } elsif ($q->{Q_usemppwidth}) {
      $reslist .= "mppwidth=".$rlist->{nodes};
    } elsif ($q->{Q_usencpus}) {
      # No nodes= if batch system allocates CPUs rather than nodes
    } else {
      $reslist .= "nodes=".$rlist->{nodes};
    }
    if ($q->{Q_usemppnppn}) {
      $reslist .= ",mppnppn=".$rlist->{tpn};
    } elsif ($q->{Q_usencpus}) {
      $reslist .= "ncpus=".$rlist->{tpn};
    } elsif (!$q->{Q_noppn}) {
      $reslist .= ":ppn=".$rlist->{tpn};
    } else {
      &logit("WARNING: Q_noppn set, but ppn=".$rlist->{tpn}) if ($rlist->{tpn} != 1);
    }
    $reslist .= $q->{Q_nodeattrib} if ($q->{Q_nodeattrib});
    printf $bf ("#PBS -l $reslist,walltime=%s\n", &secs_to_HMS($max_qtime_sec));
    printf $bf ("#PBS -o $outpath\n");
    printf $bf ("#PBS -e $errpath\n");
    printf $bf ("#PBS -q %s\n",$q->{Q_name});

    printf $bf ("\n");
    printf $bf ("QSCRIPT=$qpath\n");
    printf $bf ("RUNJOBS=${runjobs_script}\n");
    my $runlog = sprintf("%s/run_%03d.log",$logdir,$jobnum);
    unlink($runlog);
    printf $bf ("RUNLOG=$runlog\n");
    printf $bf ("TIMELIM=${max_qtime_sec}\n");
    printf $bf ("export QSCRIPT\n");
    printf $bf ("export RUNJOBS\n");
    printf $bf ("export RUNLOG\n");
    printf $bf ("export TIMELIM\n");
    printf $bf ("\n");

    output_run_env($bf);

    printf $bf ("echo Starting new batch run >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    printf $bf ("echo \"NODES\" >> \$RUNLOG\n");
    printf $bf ("if [ -n \"\$PBS_NODEFILE\" ]; then\n");
    printf $bf ("  echo PBS_NODEFILE=\$PBS_NODEFILE : >> \$RUNLOG\n");
    printf $bf ("  cat \$PBS_NODEFILE >> \$RUNLOG\n");
    printf $bf ("elif [ -n \"\$SSS_HOSTLIST\" ]; then\n");
    printf $bf ("  echo SSS_HOSTLIST=\$SSS_HOSTLIST : >> \$RUNLOG\n");
    printf $bf ("  echo \"\$SSS_HOSTLIST\" >> \$RUNLOG\n");
    printf $bf ("fi\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("env | grep PBS_ >> \$RUNLOG\n");
    printf $bf ("if [ -n \"\$PBS_JOBID\" ]; then\n");
    printf $bf ("  qstat -f \$PBS_JOBID 2>&1 >> \$RUNLOG || echo qstat failed >> \$RUNLOG\n");
    printf $bf ("fi\n");
    printf $bf ("\n");

    my $rpt = basename($run_report_file);
    printf $bf ("\$RUNJOBS $logdir ${runlistfile} $rpt \'\' \$TIMELIM $slack >> \$RUNLOG 2>&1\n");
    printf $bf ("status=\$\?\n");
    printf $bf ("echo STATUS = \$status >> \$RUNLOG\n");
    printf $bf ("if \[ \$status -eq ${AGAIN_CODE} \] ; then\n");
    printf $bf ("   echo Will resubmit job [\$QSCRIPT] >> \$RUNLOG\n");
    printf $bf ("   $resubmit_cmd \$QSCRIPT >> \$RUNLOG 2>&1\n");
    printf $bf ("fi\n\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    output_endjob_cmds($bf);
    undef $bf;

    return $rlist;
}

# =======================================================================
# Generate an SGE queue script
# =======================================================================
sub gen_sge_qscript {
    my $rlist = shift;
    
    my $jobnum = $job_number;
    $job_number++;

    my $q = $rlist->{queue};

    my $runlistfile = $rlist->{listname};
    my $jobname = get_jobname($jobnum); # No limit, but output shows only 10 chars
    my $scriptname = sprintf("qscript_%03d",$jobnum);
    my $qpath = sprintf("%s/%s",$logdir,$scriptname);
    my $outpath = sprintf("%s/out_%03d",$logdir,$jobnum);
    my $errpath = sprintf("%s/err_%03d",$logdir,$jobnum);
    my $max_qtime_sec = $rlist->{timelimit};
    my $parenv = $q->{Q_pe} || "mpich";

    my $reslist = "h_rt=" . &secs_to_HMS($max_qtime_sec);
    $reslist .= ",$q->{Q_nodeattrib}" if ($q->{Q_nodeattrib});

    $rlist->{qpath} = $qpath;

    my $bf = new IO::File("> $qpath");
    &fatal("Could not create script [$qpath]") if !defined($bf);
    chmod +(0777 & ~umask), $qpath;

    # write the header for an SGE job
    printf $bf ("#!/bin/sh\n");
    printf $bf ("#\$ -S /bin/sh\n");
    printf $bf ("#\$ -N $jobname\n");
    if ($repo && $repo ne "NA") {
      printf $bf ("#\$ -A $repo\n");
    }
    printf $bf ("#\$ -pe %s %d\n", $parenv, $rlist->{nodes});
    printf $bf ("#\$ -l $reslist\n");
    printf $bf ("#\$ -o $outpath\n");
    printf $bf ("#\$ -e $errpath\n");
    printf $bf ("#\$ -q %s\n",$q->{Q_name});

    printf $bf ("\n");
    printf $bf ("QSCRIPT=$qpath\n");
    printf $bf ("RUNJOBS=${runjobs_script}\n");
    my $runlog = sprintf("%s/run_%03d.log",$logdir,$jobnum);
    unlink($runlog);
    printf $bf ("RUNLOG=$runlog\n");
    printf $bf ("TIMELIM=${max_qtime_sec}\n");
    printf $bf ("export QSCRIPT\n");
    printf $bf ("export RUNJOBS\n");
    printf $bf ("export RUNLOG\n");
    printf $bf ("export TIMELIM\n");
    printf $bf ("\n");

    output_run_env($bf);

    printf $bf ("echo Starting new batch run >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    printf $bf ("echo \"NODES\" >> \$RUNLOG\n");
    printf $bf ("if [ -n \"\$TMPDIR\" -a -f \"\$TMPDIR/machines\" ]; then\n");
    printf $bf ("  cat \"\$TMPDIR/machines\" >> \$RUNLOG\n");
    printf $bf ("fi\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("\n");

    my $rpt = basename($run_report_file);
    printf $bf ("\$RUNJOBS $logdir ${runlistfile} $rpt \'\' \$TIMELIM $slack >> \$RUNLOG 2>&1\n");
    printf $bf ("status=\$\?\n");
    printf $bf ("echo STATUS = \$status >> \$RUNLOG\n");
    printf $bf ("if \[ \$status -eq ${AGAIN_CODE} \] ; then\n");
    printf $bf ("   echo Will resubmit job [\$QSCRIPT] >> \$RUNLOG\n");
    printf $bf ("   $resubmit_cmd \$QSCRIPT >> \$RUNLOG 2>&1\n");
    printf $bf ("fi\n\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    output_endjob_cmds($bf);
    undef $bf;

    return $rlist;
}

# =======================================================================
# Generate an RMS "allocate" script
# =======================================================================
sub gen_rms_allocate_qscript {
    my $rlist = shift;
    
    my $jobnum = $job_number;
    $job_number++;

    my $q = $rlist->{queue};

    my $runlistfile = $rlist->{listname};
    my $scriptname = sprintf("qscript_%03d",$jobnum);
    my $qpath = sprintf("%s/%s",$logdir,$scriptname);
    my $outpath = sprintf("%s/out_%03d",$logdir,$jobnum);
    my $errpath = sprintf("%s/err_%03d",$logdir,$jobnum);
    my $max_qtime_sec = $rlist->{timelimit};
    my $nodes = $rlist->{nodes};
    my $nproc = $rlist->{nproc};

    $rlist->{qpath} = $qpath;

    # Assemble arguments.
    my $submit = "$submit_cmd > $outpath 2> $errpath -N $nodes -n $nproc";
    $submit = "env RMS_TIMELIMIT=$max_qtime_sec $submit" if ($max_qtime_sec);
    $rlist->{submit_cmd} = $submit;

    my $bf = new IO::File("> $qpath");
    &fatal("Could not create script [$qpath]") if !defined($bf);
    chmod +(0777 & ~umask), $qpath;

    # write the trivial header
    printf $bf ("#!/bin/sh\n");
    printf $bf ("# to run manually: 'prun -N $nodes -n $nproc $scriptname'\n");

    printf $bf ("\n");
    printf $bf ("QSCRIPT=$qpath\n");
    printf $bf ("RUNJOBS=${runjobs_script}\n");
    my $runlog = sprintf("%s/run_%03d.log",$logdir,$jobnum);
    unlink($runlog);
    printf $bf ("RUNLOG=$runlog\n");
    printf $bf ("TIMELIM=${max_qtime_sec}\n");
    printf $bf ("export QSCRIPT\n");
    printf $bf ("export RUNJOBS\n");
    printf $bf ("export RUNLOG\n");
    printf $bf ("export TIMELIM\n");
    printf $bf ("\n");

    output_run_env($bf);

    # Ensure the allocation is utilized correctly.
    # This is needed because upcrun will default to running prun with
    # nodes==procs unless it gets a -N option.
    printf $bf ("UPCRUN_FLAGS=\"\$UPCRUN_FLAGS -N $nodes\"\n");
    printf $bf ("export UPCRUN_FLAGS\n");
    printf $bf ("\n");

    printf $bf ("echo Starting new batch run >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    printf $bf ("echo \"NODES\" >> \$RUNLOG\n");
    printf $bf ("if [ -n \"\$TMPDIR\" -a -f \"\$TMPDIR/machines\" ]; then\n");
    printf $bf ("  cat \"\$TMPDIR/machines\" >> \$RUNLOG\n");
    printf $bf ("else\n");
    printf $bf ("  prun uname -n >> \$RUNLOG\n");
    printf $bf ("fi\n");
    printf $bf ("env | grep RMS_ >> \$RUNLOG\n");
    printf $bf ("if [ -n \"\$RMS_RESOURCEID\" ]; then\n");
    printf $bf ("  rinfo \$RMS_RESOURCEID 2>&1 >> \$RUNLOG || echo rinfo failed >> \$RUNLOG\n");
    printf $bf ("fi\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("\n");

    my $rpt = basename($run_report_file);
    printf $bf ("\$RUNJOBS $logdir ${runlistfile} $rpt \'\' \$TIMELIM $slack >> \$RUNLOG 2>&1\n");
    printf $bf ("status=\$\?\n");
    printf $bf ("echo STATUS = \$status >> \$RUNLOG\n");
    printf $bf ("if \[ \$status -eq ${AGAIN_CODE} \] ; then\n");
    printf $bf ("   echo Will resubmit job [\$QSCRIPT] >> \$RUNLOG\n");
    printf $bf ("   $resubmit_cmd \$QSCRIPT >> \$RUNLOG 2>&1\n");
    printf $bf ("fi\n\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    output_endjob_cmds($bf);
    undef $bf;

    return $rlist;
}

# =======================================================================
# Generate an LSF queue script
# =======================================================================
sub gen_lsf_qscript {
    my $rlist = shift;
    
    my $jobnum = $job_number;
    $job_number++;

    my $q = $rlist->{queue};

    my $runlistfile = $rlist->{listname};
    my $jobname = get_jobname($jobnum); # No limit, but output shows only 10 chars
    my $scriptname = sprintf("qscript_%03d",$jobnum);
    my $qpath = sprintf("%s/%s",$logdir,$scriptname);
    my $outpath = sprintf("%s/out_%03d",$logdir,$jobnum);
    my $errpath = sprintf("%s/err_%03d",$logdir,$jobnum);
    my $max_qtime_sec = $rlist->{timelimit};
    my $qtime = &secs_to_HM($max_qtime_sec);

    $rlist->{qpath} = $qpath;

    my $bf = new IO::File("> $qpath");
    &fatal("Could not create script [$qpath]") if !defined($bf);
    chmod +(0777 & ~umask), $qpath;

    # write the header for an LSF job
    printf $bf ("#!/bin/sh\n");
    printf $bf ("#BSUB -J $jobname\n");
    printf $bf ("#BSUB -o $outpath\n");
    printf $bf ("#BSUB -e $errpath\n");
    printf $bf ("#BSUB -n %d\n", $rlist->{nproc});
    printf $bf ("#BSUB -W $qtime\n");
    printf $bf ("#BSUB -q %s\n", $q->{Q_name});
    if ($q->{Q_noppn}) {
      &logit("WARNING: Q_noppn set, but ppn=".$rlist->{tpn}) if ($rlist->{tpn} != 1);
    } else {
      printf $bf ("#BSUB -R 'span[ptile=%d]'\n", $rlist->{tpn});
    }

    printf $bf ("\n");
    printf $bf ("QSCRIPT=$qpath\n");
    printf $bf ("RUNJOBS=${runjobs_script}\n");
    my $runlog = sprintf("%s/run_%03d.log",$logdir,$jobnum);
    unlink($runlog);
    printf $bf ("RUNLOG=$runlog\n");
    printf $bf ("TIMELIM=${max_qtime_sec}\n");
    printf $bf ("export QSCRIPT\n");
    printf $bf ("export RUNJOBS\n");
    printf $bf ("export RUNLOG\n");
    printf $bf ("export TIMELIM\n");
    printf $bf ("\n");

    output_run_env($bf);

    printf $bf ("echo Starting new batch run >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    printf $bf ("echo \"NODES\" >> \$RUNLOG\n");
    printf $bf ("echo \$LSB_HOSTS >> \$RUNLOG\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("\n");

    my $rpt = basename($run_report_file);
    printf $bf ("\$RUNJOBS $logdir ${runlistfile} $rpt \'\' \$TIMELIM $slack >> \$RUNLOG 2>&1\n");
    printf $bf ("status=\$\?\n");
    printf $bf ("echo STATUS = \$status >> \$RUNLOG\n");
    printf $bf ("if \[ \$status -eq ${AGAIN_CODE} \] ; then\n");
    printf $bf ("   echo Will resubmit job [\$QSCRIPT] >> \$RUNLOG\n");
    printf $bf ("   $resubmit_cmd \$QSCRIPT >> \$RUNLOG 2>&1\n");
    printf $bf ("fi\n\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    output_endjob_cmds($bf);
    undef $bf;

    return $rlist;
}

# =======================================================================
# Generate a SLURM queue script
# =======================================================================
sub gen_slurm_qscript {
    my $rlist = shift;
    
    my $jobnum = $job_number;
    $job_number++;

    my $q = $rlist->{queue};

    my $runlistfile = $rlist->{listname};
    my $jobname = get_jobname($jobnum);
    my $scriptname = sprintf("qscript_%03d",$jobnum);
    my $qpath = sprintf("%s/%s",$logdir,$scriptname);
    my $outpath = sprintf("%s/out_%03d",$logdir,$jobnum);
    my $errpath = sprintf("%s/err_%03d",$logdir,$jobnum);
    my $max_qtime_sec = $rlist->{timelimit};
    my $qtime = &secs_to_HMS($max_qtime_sec);

    $rlist->{qpath} = $qpath;

    my $bf = new IO::File("> $qpath");
    &fatal("Could not create script [$qpath]") if !defined($bf);
    chmod +(0777 & ~umask), $qpath;

    # write the header for a SLURM job
    printf $bf ("#!/bin/sh\n");
    printf $bf ("#SBATCH -J $jobname\n");
    printf $bf ("#SBATCH -o $outpath\n");
    printf $bf ("#SBATCH -e $errpath\n");
    printf $bf ("#SBATCH -n %d\n", $rlist->{nproc});
    printf $bf ("#SBATCH -t $qtime\n");
    printf $bf ("#SBATCH -p %s\n", $q->{Q_name});
    if ($q->{Q_noppn}) {
      &logit("WARNING: Q_noppn set, but ppn=".$rlist->{tpn}) if ($rlist->{tpn} != 1);
    } else {
      printf $bf ("#SBATCH --ntasks-per-node=%d\n", $rlist->{tpn});
    }

    printf $bf ("\n");
    printf $bf ("QSCRIPT=$qpath\n");
    printf $bf ("RUNJOBS=${runjobs_script}\n");
    my $runlog = sprintf("%s/run_%03d.log",$logdir,$jobnum);
    unlink($runlog);
    printf $bf ("RUNLOG=$runlog\n");
    printf $bf ("TIMELIM=${max_qtime_sec}\n");
    printf $bf ("export QSCRIPT\n");
    printf $bf ("export RUNJOBS\n");
    printf $bf ("export RUNLOG\n");
    printf $bf ("export TIMELIM\n");
    printf $bf ("\n");

    output_run_env($bf);

    printf $bf ("echo Starting new batch run >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    printf $bf ("echo \"NODES\" >> \$RUNLOG\n");
    printf $bf ("echo \$SLURM_NODELIST >> \$RUNLOG\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("\n");

    my $rpt = basename($run_report_file);
    printf $bf ("\$RUNJOBS $logdir ${runlistfile} $rpt \'\' \$TIMELIM $slack >> \$RUNLOG 2>&1\n");
    printf $bf ("status=\$\?\n");
    printf $bf ("echo STATUS = \$status >> \$RUNLOG\n");
    printf $bf ("if \[ \$status -eq ${AGAIN_CODE} \] ; then\n");
    printf $bf ("   echo Will resubmit job [\$QSCRIPT] >> \$RUNLOG\n");
    printf $bf ("   $resubmit_cmd \$QSCRIPT >> \$RUNLOG 2>&1\n");
    printf $bf ("fi\n\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    output_endjob_cmds($bf);
    undef $bf;

    return $rlist;
}

# =======================================================================
# Generate an shell queue script
# =======================================================================
sub gen_shell_qscript {
    my $rlist = shift;

    my $jobnum = $job_number;
    $job_number++;

    my $nproc = $rlist->{nproc};

    my $runlistfile = $rlist->{listname};
    my $scriptname = sprintf("qscript_%03d",$jobnum);
    my $qpath = sprintf("%s/%s",$logdir,$scriptname);

    my $shell_cmd = $submit_cmd ? "via '$submit_cmd'" : 'interactively';

    $rlist->{qpath} = $qpath;
    $rlist->{timelimit} = 0;

    my $bf = new IO::File("> $qpath");
    &fatal("Could not create script [$qpath]") if !defined($bf);

    # write the header for shell script job
    printf $bf ("#!/bin/sh\n");

    printf $bf ("\n");
    printf $bf ("RUNJOBS=${runjobs_script}\n");
    my $runlog = sprintf("%s/run_%03d.log",$logdir,$jobnum);
    unlink($runlog);
    printf $bf ("RUNLOG=$runlog\n");
    printf $bf ("export RUNJOBS\n");
    printf $bf ("export RUNLOG\n");
    printf $bf ("\n");

    output_run_env($bf);

    printf $bf ("cd %s\n",$logdir);
    printf $bf ("echo Starting new batch run >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("\n");

    printf $bf ("echo Running test jobs $shell_cmd, with output to \$RUNLOG\n");
    my $rpt = basename($run_report_file);
    printf $bf ("\$RUNJOBS $logdir ${runlistfile} $rpt \'\' 0 $slack >> \$RUNLOG 2>&1\n");
    printf $bf ("status=\$\?\n");
    printf $bf ("echo STATUS = \$status >> \$RUNLOG\n");
    printf $bf ("echo >> \$RUNLOG\n");
    printf $bf ("date >> \$RUNLOG\n");
    output_endjob_cmds($bf);
    undef $bf;

    chmod 0755, $qpath;

    return $rlist;
}
 
# ======================================================================
# Submit the job to a queue for execution
# ======================================================================
sub submit_test {
    my $suite = shift;
    my $conf = shift;
    my $nthread = shift;      # number of UPC threads

    my $binary = $conf->{TestName};
    my $exitcode = $conf->{ExitCode};
    my $passexpr = $conf->{PassExpr};
    my $failexpr = $conf->{FailExpr};
    my $runtime = int($timeout_multiplier *
		      (defined($conf->{TimeLimit}) ? $conf->{TimeLimit} : $default_runtime));
    my $app_args = defined($conf->{AppArgs}) ? $conf->{AppArgs} : '';
    my $app_env = defined($conf->{AppEnv}) ? $conf->{AppEnv} : '';
    my $runknown = get_runknown_str($conf);
    my $benchmark = defined($conf->{BenchmarkResult}) ? $conf->{BenchmarkResult} : '';

    my $npthread = $num_pthreads;

    my $nproc = $nthread;     # how many processes to use?
    if ($npthread > 0) {
	if ($npthread > $nthread) {
	   $npthread = $nthread;
	   &logit("WARNING: Setting num_pthreads to $nthread for [$suite/$binary]");
	}
	$nproc = int(($nthread + $npthread - 1)/$npthread);
    }

    my $saveoutput = 0;
    if (defined($conf->{SaveOutput})) {
	$saveoutput = $conf->{SaveOutput};
    } elsif ($benchmark) {
	$saveoutput = 1;
    }
   
    my $runcmd = $conf->{RunCmd};
    $runcmd = $upcrun if (!defined($runcmd));
    if (defined($conf->{RunCmdArgs})) {
      my $runargs = $conf->{RunCmdArgs};
      $runcmd =~ s/%B/%B $runargs/;
    }

    my $run_entry = "${current_run_dir} \'$runcmd\' $binary $runtime $exitcode " .
                    "\'$passexpr\' \'$failexpr\' $nthread $npthread " .
                    "$keep_binary $saveoutput \'$app_args\' \'$app_env\' \'$runknown\' \'$benchmark\'\n";

    # select a queue script for running the job
    my $rlist = &select_runlist($nproc,$runtime);
    if (defined($rlist)) {
	my $fh = $rlist->{fh};
	$rlist->{numjobs}++;
	$rlist->{totaltime} += $runtime;
	my $cnt = $rlist->{numjobs};

	print $fh ($run_entry);

	my $rlname = $rlist->{listname};
	&logit("Submitting [$suite/$binary] as $cnt job to $rlname");
    } else {
	# No queue to handle this request, add it to NoQueueList file
	# and issue warning
	if (! defined($no_queue_list)) {
	    my $no_queue_path = "$logdir/NoQueueList";
	    my $fh = new IO::File("> $no_queue_path");
	    &fatal("Could not create file [$no_queue_path]") if !defined($fh);
	    $no_queue_list = {'listname'  => "NoQueueList",
			      'fh'        => $fh,
			      'numjobs'   => 0
			      };
	}
	my $fh = $no_queue_list->{fh};
	my $rlname = $no_queue_list->{listname};
	$no_queue_list->{numjobs}++;
	print $fh ($run_entry);

	&logit("WARNING: No queue for [$suite/$binary], $nproc processes, adding to $rlname");
    }
    
}

#   remove the binary and .o files if they exist    
sub remove_binary {
    my $conf = shift;
    my $files = $conf->{Files};
    my $binary = $conf->{TestName};
    my $binary_gdb = $binary . ".gdb";
    unlink($binary) if (-e $binary);
    unlink($binary_gdb) if (-e $binary_gdb);
    my $file;
    foreach $file (split(/\s+/,$files)) {
	$file =~ s/\.[a-z]+$//;
	$file .= ".o";
        if (-e $file) {
	  print "Removing $file\n" if $debug;
	  unlink($file) or die "Failed to remove '$file': $!\n";
        }
    }
}

# ======================================================================
# Compile the testcase.
# Note the return value of 1 means to continue on to run the code.
# A return value of 0 means NOT to run the code.
# Note that some tests are designed to fail compilation.  They are
# successful if the compilation produces errors, but you still dont
# want to (or cannot) run the resulting code.
# ======================================================================
sub compile_test {
    my $suite = shift;
    my $conf = shift;

    my $flags = $conf->{Flags};
    my $upcflags = "";
    if (have_feature('upcr')) {
      $upcflags .= "-network=$network -nolines";
    }
    my $files = $conf->{Files};
    my $binary = $conf->{TestName};
    my $nolink = defined($conf->{NoLink});
    my $expect = "pass";
    if (defined($conf->{CompileResult})) {
	$expect = $conf->{CompileResult};
    }
    my $do_trans = (! defined($conf->{NoTrans})) && ($compiler_spec{upc_trans_option} ne "");

    &remove_binary($conf);

    my $cmd;
    my $bldcmd = $conf->{BuildCmd};
    if ($bldcmd eq "upcc") {
	$upcflags .= " -pthreads -nolink-cache" if ($num_pthreads > 0);
	if ($nolink) {
	    $cmd = "$upcc $upcflags $flags -c $files";
	} else {
	    $cmd = "$upcc $upcflags $flags -o $binary $files";
	}
    } else {
	# must be "make"
	if ($num_pthreads > 0) {
	    $upcflags .= " -pthreads -nolink-cache";
	}
	my $makeflags = "";
	if (defined($conf->{MakeFlags})) {
	    $makeflags .= sprintf(" %s",$conf->{MakeFlags});
	}
        my $stat_threads = $compiler_spec{upc_static_threads_option};
        chomp($stat_threads);
        $stat_threads =~ s/%T//g;
	my $exesuffix;
	$exesuffix = "EXESUFFIX=".$compiler_spec{exe_suffix} if ($compiler_spec{exe_suffix});
	$cmd = "$make $makeflags UPCC=\'$upcc $upcflags $flags\' NETWORK=$network UPCC_STAT_THREADS='$stat_threads' $exesuffix $binary";
    }

    sub do_careful_compile($) {
      my $cmd = shift;
      my $outfile = get_tmpfilename();
      my $errfile = get_tmpfilename();
      my ($status,$got_timeout) = runjob_withtimeout(sub {
        execcmd($cmd, $outfile, $errfile);
      }, $timelimit*$timeout_multiplier);
      my $out = "";
      foreach my $file ($outfile, $errfile) {
        open(GETOUT, "<$file") or die "Can't open temp file $file: $!\n";
        my $oldslash = $/;
        undef $/; 
        $out .= <GETOUT>; # slurp!
        $/ = $oldslash;
        close GETOUT;
        unlink "$file" or die "Failed to delete '$file': $!\n";
      }
      $out =~ s/\n\n/\n/g; # strip blank lines from output
      if ($got_timeout) {
        $out .= "ERROR: Compilation timed out - killed after ".$timelimit*$timeout_multiplier." seconds\n";
      }
      return ($status,$out);
    }

    &logit("Compiling [$cmd]");
    my ($status,$out) = do_careful_compile($cmd);
    my $cwd = getcwd();
    $out = "cd $cwd\n$cmd\n".$out; # include cwd && command in output
    my $filtered_out = $out;
    $filtered_out =~ s/$warning_blacklist//mg;

    # put output of compile command to log file regardless of success
    # or failure
    print $logf $out if (defined($logf));

    my $got_error = ($filtered_out =~ /error/i);
    my $got_warning = ($filtered_out =~ /warning/i);
    my $expect_fail = ($expect =~ /fail/i);
    my $expect_pass = ($expect =~ /pass/i);

    printf $compile_rpt $DASHLINE;
    #my $str = sprintf("%-10s %-30s",$suite,$binary);
    my $str = "[$suite/$binary] ";

    # Report if status and output are not consistent
    if (!grep(/^berkeleyupc$/i,@compiler_features)) {
        # except on non-upcc compilers that may have diff err output conventions (eg xlupc)
        $got_error = $got_error || $status;
    } elsif ($status && !($got_error || $got_warning)) {
	$str .= "FAILED: compile exit=$status, no conforming error/warning\n";
	$str = check_known($str, $conf, 'compile-failure');
	print $compile_rpt $str;
	print $compile_rpt $out;
	$compile_rpt->flush();
	return 0;
    } elsif ($got_error && !$status) {
	$str .= "FAILED: compile exit=0, but generated error\n";
	$str = check_known($str, $conf, 'compile-failure');
	print $compile_rpt $str;
	print $compile_rpt $out;
	$compile_rpt->flush();
	return 0;
    }

    my $got_trans_error = 0;
    my $got_trans_crash = 0;
    my $out_2 = "";
    if ($got_error || $got_warning) {
     if (!$do_trans) { 
	$got_trans_error = $got_error; # one-step compilation
	$out_2 = $out;
     } else {
	# try just the UPC-to-C translation
	$upcflags .= " ". $compiler_spec{upc_trans_option};
	if ($bldcmd eq "upcc") {
	    $cmd = "$upcc $upcflags $flags $files";
	} else {
	    # we require that an _trans target exists in the
	    # makefile
	    my $target = "${binary}_trans";
	    my $makeflags = "";
	    if (defined($conf->{MakeFlags})) {
		$makeflags .= sprintf(" %s",$conf->{MakeFlags});
	    }
	    $cmd = "$make $makeflags UPCC=\'$upcc $upcflags $flags\' $target";
	}
	&logit("Translating [$cmd]");
	my $status2;
	($status2,$out_2) = do_careful_compile($cmd);

        # put output of compile command to log file regardless of success
        # or failure
        print $logf $out_2 if (defined($logf));

	$got_trans_error = ($out_2 =~ /error/i);
	$got_trans_crash = ($out_2 =~ /sgiupc INTERNAL ERROR:/) || ($out_2 =~ /Internal compiler error/);

	# Report if status and output are not consistent
	if ($status2 && !($out_2 =~ /error|warning/i)) {
	    $str .= "FAILED: compile exit=$status2, no conforming error/warning\n";
	    $str = check_known($str, $conf, 'compile-failure');
	    print $compile_rpt $str;
	    print $compile_rpt $out_2;
	    $compile_rpt->flush();
	    return 0;
	} elsif ($got_trans_error && !$status2) {
	    $str .= "FAILED: compile exit=0, but generated error\n";
	    $str = check_known($str, $conf, 'compile-failure');
	    print $compile_rpt $str;
	    print $compile_rpt $out_2;
	    $compile_rpt->flush();
	    return 0;
	}
      }
    }

    if ($got_error) {
	if ($got_trans_crash) {
	    # crash cannot ever be the expected behavior
	    $str .= "FAILED: expect " . ($expect_fail ? "fail" : "pass") . " but got translator crash\n";
	    $str = check_known($str, $conf, 'compile-failure');
	} elsif ($got_trans_error) {
	    if ($expect_fail) {
		$str .= "SUCCESS: expect fail and got error\n";
	    } else {
		$str .= "FAILED: expect pass but got error\n";
	        $str = check_known($str, $conf, 'compile-failure');
	    }
	    $out = $out_2;
	} else {
	    $str     .= "FAILED: UPC-To-C Translation or Link error\n";
	    $str = check_known($str, $conf, 'compile-failure');
	}
	print $compile_rpt $str;
	print $compile_rpt $out;
	$compile_rpt->flush();
	return 0;
    }
    if (!$nolink && ! -x $binary) {
	# did not detect failure, but binary is not executable
	$str .= "FAILED: no binary generated\n";
	$str = check_known($str, $conf, 'compile-failure');
	print $compile_rpt $str;
	print $compile_rpt $out;
	$compile_rpt->flush();
	return 0;
    }
    # no compiler error, and binary exists
    if ($expect_fail) {
	$str .= "FAILED: expect fail but passed\n";
	$str = check_known($str, $conf, 'compile-failure');
	print $compile_rpt $str;
	print $compile_rpt $out;
	$compile_rpt->flush();
	return 0;
    }
    if ($got_warning) {
	$str .= "FAILED: expect pass but got warning\n";
	$str = check_known($str, $conf, 'compile-warning');
	print $compile_rpt $str;
	print $compile_rpt $out;
	$compile_rpt->flush();
	return 1;
    }
    $str .= "SUCCESS: expect pass and passed\n";
    print $compile_rpt $str;
    $compile_rpt->flush();
    
    return 1;
}     

# ======================================================================
# Convert a string of digits seperated by commas into an array
# ======================================================================
sub extract_array {
    my $str = shift;

    if (! defined($str)) {
	return undef;
    }

    
    $str =~ s/\#.*$//;          # delete trailing comment
    $str =~ s/\s+//g;           # delete white space

    my $arr = [];
    my $item;
    foreach $item (split(',',$str)) {
	if ($item =~ /\d+/) {
	    push(@$arr,$item) if ($item > 0);
	} else {
	    &fatal("Expected number, got [$item]");
	}
    }

    return ( (scalar(@$arr) > 0) ? $arr : undef );
}

# ======================================================================
# Read compiler spec
# ======================================================================
sub parse_compiler_spec {
    my ($config_file, %compiler_spec) = @_;

    if (!open(CONFIG, $config_file)) {
       die "Compiler spec file '$config_file' is missing\n"
    } else {
       &logit("Reading compiler spec file [$config_file]");
    }

    my $linenum = 0;
    my $line = "";
    my $errname ="";
    while (<CONFIG>) {
        $linenum++;
        $errname = "${config_file}:${linenum}::" if ($line eq "");
        my $newline = "$_";
        $newline =~ s/^\s+//;            # chop leading whitespace
        $newline =~ s/^#.*//;            # drop comment lines (full lines only)
        if ($newline =~ /.*\\$/) {       # backslash line continuation
           $newline =~ s/\\$//;          # chop trailing backslash
           $newline =~ s/\s+$//;         # chop trailing whitespace
           $line = $line . $newline;
           next;
        }
        $newline =~ s/\s+$//;            # chop trailing whitespace
        $line = $line . $newline;
        next unless length($line);       # ignore empty lines
        unless ($line =~ /^\s*\w+\s*=/) {
            die "$errname Invalid line (no '=', nor a comment):\n $line\n";
        }
        # Split into 2 parts at first '='; Allow spaces around '='.
        my ($var, $val) = split /\s*=\s*/, $line, 2;
        # Read each setting, checking to see that it's a valid variable name. 
        unless (defined($compiler_spec{$var})) {
            die "$errname unknown compiler spec setting '$var'\n";
        }
	# expand $var$ variables
	my $parity = 0;
	foreach my $poss_var (split(/\$/,$val)) {
	  $parity = !$parity;
          next if ($parity);
	  if (my $exp = $compiler_spec{$poss_var} || $ENV{$poss_var}) {
	     $val =~ s/\$$poss_var\$/$exp/;
          }
	}
	if ($var eq "upc_home" && $upc_home_override) {
          $compiler_spec{$var} = $upc_home_override;
	} else {
          $compiler_spec{$var} = $val;
	}
        $line = "";
    }
    close CONFIG;
    unless ($line eq "") {
        die "$errname unterminated line at EOF:\n $line\n";
    }
    # check to see all variables without default values have been set 
    for my $key (sort keys(%compiler_spec)) {
	print "compiler_spec{$key} = " . $compiler_spec{$key} . "\n" if $debug;
        if ($compiler_spec{$key} eq 'nodefault') {
            die "Setting for '$key' missing from config file '$compiler_spec_file' for '$key'\n";
        }
    }
    return %compiler_spec;
}

# ======================================================================
# Generate an array of test configurations from the configuration
# file.
# ======================================================================
sub read_suite_config {
    my $suite = shift;
    my $file = shift;
    my $path = "$suite/$file";

    my $fh = new IO::File("< $file");
    if (! defined($fh)) {
	&logit("Could not open harness.conf in $suite\n");
	return 0;
    }
    &logit("Reading configuration file for suite $suite");

    # NOTE: Keep spec records in both hash, for quick searching
    #       by TestName, and in array, to keep output in order
    my %spec_hash = ();
    my @spec_arr = ();
    my $default = { 'SuiteName' => "$suite" };
    my $curspec = undef;
    my $indefault = 0;
    my $line = 0;
    my $nospec = {};

    while (<$fh>) {
	$line += 1;
	s/^\s+//;        # skip leading white space
	next if /^$/;    # skip blank lines
	next if /^\#/;   # skip comments

	chomp;           # get rid of EOLN

	if (/^BEGIN_DEFAULT_CONFIG/) {
	    $default = { 'SuiteName' => "$suite" };
	    $curspec = $default;
	    $indefault = 1;
	    next;
	}
	if (/^END_DEFAULT_CONFIG/) {
	    $indefault = 0;
	    $curspec = undef;
	    next;
	}

	if (! /^(\S+)\:(.*)$/) {
	    &syntax("Invalid line [$_] at [$path:$line]", undef);
	}

	my $key = $1;
	my $val = $2;
	$val =~ s/\#.*$//; # strip trailing comment
	$val =~ s/^\s+//;  # strip leading whitespace
	$val =~ s/\s+$//;  # strip trailing whilespace
	if ($key eq "WildCard") {
	    if ($indefault) {
		&syntax("WildCard not allowed in DEFAULT section at [$path:$line]", undef);
	    }
	    # generate a set of specs from file wildcards
	    if ($val =~ /^(.*)\<(.*)\>(.*)$/) {
		my $pre = $1;
		my $pat = $2;
		my $post = $3;

		my @list = <${pre}${pat}${post}>;

		my $file;
		foreach $file (@list) {
		    my $test = $file;
		    $test =~ s/^${pre}//;
		    $test =~ s/${post}$//;
		    if (defined($spec_hash{$test})) {
			&logit("WildCard generated duplicate spec for [$test], ignoring");
		    } else {
			my $spec;
			%$spec = %$default;
			$spec->{'TestName'} = $test;
			$spec->{'Files'} = $file;
			parse_compilerspec_failure($suite, $test, "[$path:$line]", $spec);
			$spec_hash{$test} = $spec;
			push(@spec_arr,$spec);
		    }
		}
	    }
	    $curspec = undef;
	    next;
	}

	# check the key name is valid
	if (! grep(/^$key$/,@VALID_TESTSUITE_KEYWORDS)) {
	    &syntax("Unknown keyword [$key] at [$path:$line]\n".
                    "Valid keywords are: ".join(',',@VALID_TESTSUITE_KEYWORDS), $curspec);
	}
	
	if ($key eq "TestName") {
	    if ($indefault) {
		&syntax("TestName not allowed in DEFAULT section at [$path:$line]", undef);
	    }
	    # Starting new test specification,
	    # check if we have seen it before
	    if (defined($spec_hash{$val})) {
		$curspec = $spec_hash{$val};
	    } else {
		my $newspec = {};
		%$newspec = %$default;
		$newspec->{TestName} = $val;
		$spec_hash{$val} = $newspec;
		push(@spec_arr,$newspec);
		$curspec = $newspec;
	        parse_compilerspec_failure($suite, $val, "[$path:$line]", $newspec);
	    }
	    next;
	} elsif ($key eq "SaveOutput") {
	    if ($val !~ /^\d+$/) {
		&syntax("Invalid value (extected digit) in line [$_] at [$path:$line]", $curspec);
	    }
	} elsif ($key eq "CompileResult") {
	    $val = lc($val);
	    if (! grep(/^$val$/,@VALID_COMPILER_RESULTS)) {
		&syntax("Invalid value in line [$_] at [$path:$line]", $curspec);
	    }
	} elsif ($key eq "BuildCmd") {
	    $val = lc($val);
	    my $v;
	    my $newval = undef;
	    $val =~ s/\s*\,\s*/,/g;
	    $val =~ s/\s+/,/g;
	    foreach $v (split(",",$val)) {
		if ($v eq "nolink") {
		    $curspec->{NoLink} = 1;
		} elsif ($v eq "notrans") {
		    $curspec->{NoTrans} = 1;
		} elsif (grep(/^$v$/,@VALID_BUILD_COMMANDS)) {
		    $newval = $v;
		} else {
		    &syntax("Invalid BuildCmd [$v] at [$path:$line]", $curspec);
		}
	    }
	    if (!defined($newval)) {
		&syntax("BuildCmd underspecified [$val] at [$path:$line]", $curspec);
	    }
	    # we set the key/value pair below
	    $val = $newval
	} elsif ($key eq "RunCmd") {
	    # nothing to do
	} elsif ($key eq "RunCmdArgs") {
	    # nothing to do
	} elsif ($key eq "TimeLimit") {
	    $val =~ s/\$DEFAULT\$/$default_runtime/g;
	    # convert value to time in seconds
	    if ($val =~ /^\d+$/) {
		# all digits, assume seconds
	    } elsif ($val =~ /^(\d+):(\d+)$/) {
		# min:sec
		$val = 60*$1 + $2;
	    } elsif ($val =~ /^(\d+):(\d+):(\d+)$/) {
		# hour:min:sec
		$val = 3600*$1 + 60*$2 + $3;
	    } else {
		&syntax("Invalid time value [$val] in line [$_] at [$path:$line]", $curspec);
	    }
	} elsif ($key eq "AppArgs") {
	    # strip surrounding quotes if necessary
	    if ($val =~ /^[\"\'](.*)[\"\']$/) {
		$val = $1;
	    }
	    # don't bother recording if no args
	    next if (!defined($val) || ($val =~ /^\s*$/));
        } elsif ($key eq "AppEnv") {
	    $val =~ s/\'/\\\'/g; # quote single quotes, which we use to pass in runlist
	    # don't bother recording if no args
	    next if (!defined($val) || ($val =~ /^\s*$/));
        } elsif ($key eq "KnownFailure") {
            if ($indefault) {
                &syntax("KnownFailure not allowed in DEFAULT section at [$path:$line]", undef);
            }
            if ($compiler_spec{known_failures}) {
                next; # compiler spec overrides harness.conf
            }
            # strip surrounding quotes if necessary
            if ($val =~ /^[\"\'](.*)[\"\']$/) {
                $val = $1;
            }
            parse_known($val,"[$path:$line]",$curspec);
            next;
	} elsif ($key eq "RequireFeature") {
	    # strip surrounding quotes if necessary
	    if ($val =~ /^[\"\'](.*)[\"\']$/) {
		$val = $1;
	    }
	} elsif ($key eq "ProhibitFeature") {
	    # strip surrounding quotes if necessary
	    if ($val =~ /^[\"\'](.*)[\"\']$/) {
		$val = $1;
	    }
	} elsif ($key eq "BenchmarkResult") {
	   $curspec->{BenchmarkResult} = $val; 
	}
	
	if (! defined($curspec)) {
	    &syntax("Floating spec [$key : $val] at [$path:$line]", $nospec);
	}

	$curspec->{$key} = $val;
    }

    close($fh);

    # Each specification may give rise to multiple configurations.
    # Generate a list of all configurations here

    my $configs = [];

    my $spec;
    foreach $spec (@spec_arr) {
	# expand wildcard patters in specification
	$spec = &filter_conf($spec);

	# check that several key fields are defined
	next if (!defined($spec->{TestName}));
	my $test = $spec->{TestName};

	# Expand list of static threads to multiple tests
	my $arr = &extract_array($spec->{StaticThreads});
	if (defined($arr)) {
	    my $nth;
	    foreach $nth (@$arr) {
		my $conf = {};
		%$conf = %$spec;
		$conf->{TestName} = sprintf("%s_st%02d",$test,$nth);
		delete($conf->{DynamicThreads});
		$conf->{StaticThreads} = $nth;
		$conf->{MakeFlags} .= " UPCTHREADS=${nth}";
	        my $stat_threads = $compiler_spec{upc_static_threads_option};
	        $stat_threads =~ s/%T/${nth}/g;
		$conf->{Flags} .= " " . $stat_threads;
		push(@$configs,$conf);
	    }
	}

	# use this spec structure for the dynamic configuration record
	delete($spec->{StaticThreads});

	$arr = &extract_array($spec->{DynamicThreads});
	if (defined($arr)) {
#	    $spec->{DynamicThreads} = $arr;
	    push(@$configs,$spec);
	}
    }

    if ($debug) {
	my $conf;
	foreach $conf (@$configs) {
	    &dumpconf($conf);
	}
    }

    return $configs;

    
}

# ======================================================================
# This function gets called to expand wildcard values in the
# configuration definitions
# ======================================================================
sub filter_conf {
    my $c = shift;
    my ($key,$val);

    my $tname = $c->{TestName};
    while (($key,$val) = each %$c) {
	$val =~ s/\$TESTNAME\$/$tname/g;
	if ($val =~ /\$DEFAULT\$/) {
	    if ($key eq "TimeLimit") {
		$val = $default_runtime;
	    } else {
		$val =~ s/\$DEFAULT\$/$nthread_default/g;
	    }
	}
	# now, expand environment vars found on the RHS
	while ($val =~ /(\$(\w+)\$)/) {
	    my $str = $1;
	    my $sym = $2;
	    if (defined($ENV{$sym})) {
		$val =~ s/\$$sym\$/$ENV{$sym}/g;
	    } else {
		&fatal("Undefined macro/env_var in [$key=$val] of harness");
	    }
	}
	$c->{$key} = $val;
    }
    return $c;
}

# ======================================================================
# Write configuration to stdout for debug
# ======================================================================
sub dumpconf {
    my $c = shift;
    my $key = "TestName";
    my $val = $c->{$key};
    printf ("%-15s = %s\n",$key,$val);
    foreach $key (sort keys %$c) {
	next if ($key eq "TestName");
	$val = $c->{$key};
        if ($key eq "KnownFailure") {
	  foreach my $failtype (@{$val}) {
 	    printf ("%-15s = %s\n",$key,join(';',@{$failtype}));
          }
        } else {
 	  printf ("%-15s = %s\n",$key,$val);
        }
    }
    print "\n";
}


# ======================================================================
# Open and parse the system configuration file
# ======================================================================
sub parse_sysconfig {
    my $file = shift;

    # Start with some defaults
    my $conf = {};

    my $fh;
    $fh = new IO::File("< $file") if (-e $file);
    $fh = new IO::File("< $harness_src_path/sysconfs/$file") if (!defined($fh) && -e "$harness_src_path/sysconfs/$file");
    &fatal("Can't open sys config file [$file]") if (!defined($fh));

    # Step 1: slurp contents into an array, filtering out
    # comments and extranious white space
    my @tokens = ();
    my $linenum = 0;
    while (<$fh>) {
	$linenum++;
	# get rid of comments
	s/\#.*$//;
	# skip blank lines
	next if /^\s*$/;
	# skip leading and trailing white space
	s/^\s+//;
	s/\s+$//;
        # get rid of eoln
	chomp();

	# split line into tokens
	while (1) {
	    s/^\s+//;
	    last if /^$/;
	    if (/^define\s+(\w+)\s*=\s*(\S+)\s*$/i) {
		$SYS_ENV{$1} = $2;
	    } elsif (/^\'([^\']*)\'/) {
		# a quoted string
		push(@tokens,[$TOK_VALUE,"$1"]);
	    } elsif (/^(\d(\d|\:)*\d)/) {
		push(@tokens,[$TOK_VALUE,$1]);
	    } elsif (/^(\d+(\.\d*)?)/) {
		push(@tokens,[$TOK_VALUE,$1]);
	    } elsif (/^[\,\;]/) {
		# skip seperators
	    } elsif (/^((\%|\w|\/|\-|\:|\.)+)/) {
		push(@tokens,[$TOK_NAMEVAL,$1]);
#		my $v = $1;
#		while ($v =~ /(\%([A-Z]+)\%)/) {
#		    my $str = $1;
#		    my $sym = $2;
#		    if (defined($SYS_ENV{$sym})) {
#			$v =~ s/$str/$SYS_ENV{$sym}/g;
#		    } elsif (defined($ENV{$sym})) {
#			$v =~ s/$str/$ENV{$sym}/g;
#		    } else {
#			&fatal("Env var [$sym] not defined");
#		    }
#		}
#		push(@tokens,[$TOK_NAMEVAL,$v]);
	    } elsif (/^(\=\>?)/) {
		push(@tokens,[$TOK_ASSIGN,$1]);
	    } elsif (/^(\[)/) {
		push(@tokens,[$TOK_ARRAYBEGIN,$1]);
	    } elsif (/^(\])/) {
		push(@tokens,[$TOK_ARRAYEND,$1]);
	    } elsif (/^(\{)/) {
		push(@tokens,[$TOK_HASHBEGIN,$1]);
	    } elsif (/^(\})/) {
		push(@tokens,[$TOK_HASHEND,$1]);
	    } else {
		&fatal("Invalid sys config tokens in line $linenum [$_] of $file");
	    }
	    # eat portion of line not matched
	    $_ = $';    #rest of pattern'
	}
    }

    &dump_tokens(\@tokens) if $debug;

    while (1) {

	my ($name,$val) = &get_defn(\@tokens);

	last if (!defined($name));

	$val = &expand_env($val);
	$conf->{$name} = $val;

	if ($debug) {
	    printf("Sysconf: Defined %s = ",$name);
	    &print_val($val);
	    printf("\n");
	}
    }

    # check that all requires values have been defined
    my $required;
    foreach $required (@REQUIRED_SYSCONFIG_FIELDS) {
	if (! defined($conf->{$required}) ) {
	    &fatal("Required field [$required] not defined in sys config file [$file]");
	}
    }

    return $conf;
}

sub expand_env_array {
    my $arr = shift;
    my $v;
    my $narr = [];
    foreach $v (@$arr) {
	push(@$narr,&expand_env($v));
    }
    return $narr;
}
sub expand_env_hash {
    my $arr = shift;
    my $n;
    my $v;
    my $narr = {};
    while (($n,$v) = each (%$arr)) {
	$narr->{$n} = &expand_env($v);
    }
    return $narr;
}
sub expand_env {
    my $val = shift;
    if (ref($val) eq "ARRAY") {
	return &expand_env_array($val);
    } elsif (ref($val) eq "HASH") {
	return &expand_env_hash($val);
    }
    while ($val =~ /(\%(\w+)\%)/) {
	my $str = $1;
	my $sym = $2;
	if (defined($SYS_ENV{$sym})) {
	    $val =~ s/$str/$SYS_ENV{$sym}/g;
	} elsif (defined($ENV{$sym})) {
	    $val =~ s/$str/$ENV{$sym}/g;
	} else {
	    &fatal("Undefined macro/env_var in [$val] of sysconf file");
	}
    }
    return $val;
}

sub print_array {
    my $arr = shift;
    printf("[");
    my $v;
    foreach $v (@$arr) {
	print_val($v);
    }
    printf(" ]");
}
sub print_hash {
    my $arr = shift;
    printf("{");
    my $n;
    my $v;
    while (($n,$v) = each(%$arr)) {
	printf(" $n =>");
	print_val($v);
    }
    printf(" }");
}
sub print_val {
    my $val = shift;
    if (ref($val) eq "ARRAY") {
	&print_array($val);
    } elsif (ref($val) eq "HASH") {
	&print_hash($val);
    } else {
	printf(" %s",$val);
    }
}

sub dump_tokens {
    my $tokens = shift;

    my $tok;
    foreach $tok (@$tokens) {
	printf("TOKEN  %-12s [%s]\n",$TOK_NAMES[$tok->[0]],$tok->[1]);
    }
}

# ======================================================================
# 
# ======================================================================
sub get_defn {
    my $tokens = shift;

    my $numtok = scalar(@$tokens);
    return (undef,undef) if ($numtok == 0);

    if ($numtok < 3) {
	# must have name = value (3 tokens)
	&dump_tokens($tokens) if $debug;
	&fatal("Invalid number of tokens left in sys config file");
    }

    my $nametok = shift(@$tokens);
    my $septok  = shift(@$tokens);
    my $valtok  = shift(@$tokens);

    my $name = $nametok->[1];
    my $val =  $valtok->[1];

    if ($nametok->[0] != $TOK_NAMEVAL) {
	&fatal("Expected name, got [$name] in sys config file");
    }
    
    if ($septok->[0] != $TOK_ASSIGN) {
	my $sep = $septok->[1];
	&fatal("Expected seperator, got [$sep] for [$name] in sys config file");
    }
    if ($valtok->[0] == $TOK_VALUE) {
    } elsif ($valtok->[0] == $TOK_NAMEVAL) {
    } elsif ($valtok->[0] == $TOK_ARRAYBEGIN) {
	$val = &get_array($tokens);
    } elsif ($valtok->[0] == $TOK_HASHBEGIN) {
	$val = &get_hash($tokens);
    } else {
	&fatal("Invalid value [$val] for [$name] is sys config file");
    }

    return ($name,$val);
}
    
sub get_hash {
    my $tokens = shift;

    my $hash = {};

    while (1) {
	my $numtok = scalar(@$tokens);
	if ($numtok == 0) {
	    &fatal("No tokens left in get_hash");
	}
	my $tok = $tokens->[0];
	if ($tok->[0] == $TOK_HASHEND) {
	    shift(@$tokens);
	    return $hash;
	}

	my ($name,$val) = &get_defn($tokens);

	if (! defined($name)) {
	    &fatal("Expected (name,value) pair in hash");
	}
	$hash->{$name} = $val;
    }
}
sub get_array {
    my $tokens = shift;

    my $arr = [];

    while (1) {
	my $numtok = scalar(@$tokens);
	if ($numtok == 0) {
	    &fatal("No tokens left in get_array");
	}
	my $tok = $tokens->[0];
	if ($tok->[0] == $TOK_ARRAYEND) {
	    shift(@$tokens);
	    return $arr;
	}

	$tok = shift(@$tokens);
	my $toktype = $tok->[0];
	my $val;

	if ($toktype == $TOK_ARRAYBEGIN) {
	    $val = &get_array($tokens);
	} elsif ($toktype == $TOK_HASHBEGIN) {
	    $val = &get_hash($tokens);
	} elsif ($toktype == $TOK_VALUE) {
	    $val = $tok->[1];
	} elsif ($toktype == $TOK_NAMEVAL) {
	    $val = $tok->[1];
	} else {
	    &fatal("Invalid toktype [$toktype] in get_array");
	}

	push(@$arr,$val);
    }
}
    
# ======================================================================
# remove the files that match the given pattern.
# ======================================================================
sub rm_files {
    my $pattern = shift;
    my @files = glob($pattern);
    unlink @files;
    &logit("Unlinked files that match pattern $pattern");
}

# ======================================================================
# Run a system command and log output to log file.
# Die if command returs non-zero exist status.
# ======================================================================
sub run_cmd {
    my $cmd = shift;
    my $out = `$cmd 2>&1`;
    my $status = $?;
    print $logf $out;
    if ($status != 0) {
	&fatal("FAILED: running [$cmd]");
    }
    my $dir = cwd();
    &logit("Completed [$cmd] in [$dir]");
}

# ======================================================================
# parse the command line args
# ======================================================================
sub parse_args {
    my $opt_help = 0;
    my $norun = 0;
    my $norc = 0;
    my $nocompile = 0;
    my $dryrun = 0;
    my $sysconf_file = undef;
    my @suite = ();
    my @test = ();
    my @includefilter = ();
    my @excludefilter = ();
    my $npthread = undef;
    my $upcthread = undef;
    my $repoacct = undef;
    my $ppn = undef;
    my $conduit = undef;
    my $lpath = undef;
    my $wpath = undef;
    my $max_nodes = undef;
    my $compiler_featurelist = undef;
    my $shell_cmd = undef;
    my $submit_threshold = undef;
    my $status = GetOptions(
			    'help'           => \$opt_help,
			    'debug'          => \$debug,
			    'g'              => \$upcc_debug,
			    'tv'             => \$upcc_tv,
			    'profile'        => \$upcc_profile,
			    'profile-local'  => \$upcc_profile_local,
			    'nocompile'      => \$nocompile,
			    'norc'           => \$norc,
			    'norun'          => \$norun,
			    'compileonly'    => \$compileonly,
			    'dryrun'         => \$dryrun,
			    'sysconf=s'      => \$sysconf_file,
			    'compiler_spec=s'=> \$compiler_spec_file,
			    'upc_home=s'     => \$upc_home_override,
			    'threads=i'      => \$upcthread,
			    'pthreads=i'     => \$npthread,
			    'ppn=i'          => \$ppn,
			    'suite=s'        => \@suite,
			    'test=s'         => \@test,
			    'include=s'      => \@includefilter,
			    'exclude=s'      => \@excludefilter,
			    'filepat=s'      => \$test_file_pat,
			    'repo=s'         => \$repoacct,
			    'network=s'      => \$conduit,
			    'logdir=s'       => \$lpath,
			    'workdir=s'      => \$wpath,
			    'runlimit=i'     => \$default_runtime,
			    'clean'          => \$clean_build,
			    'keep!'          => \$keep_binary,
	                    'features=s'     => \$compiler_featurelist,
			    'max_node_run=i' => \$max_nodes,
			    'timeout_multiplier=s' => \$timeout_multiplier,
			    'O'              => \$optimize,
			    'opt'            => \$trans_opt,
			    'shell_cmd=s'    => \$shell_cmd,
			    'submit_threshold=i' => \$submit_threshold,
			    );

    if (! $status) {
	# Get Options failed, report failure and die.
	printf("Error: parse_args\n");
	exit(1);
    }

    my $gotone = 0;
    foreach my $spec_file ("$compiler_spec_file", 
                           "$harness_bld_path/$compiler_spec_file", 
                           "$harness_src_path/$compiler_spec_file") {
      if (-f $spec_file) {
        %compiler_spec = parse_compiler_spec($spec_file, %compiler_spec);
	$gotone = 1; last;
      } elsif (-f "$spec_file.spec"){
        %compiler_spec = parse_compiler_spec("$spec_file.spec", %compiler_spec);
	$gotone = 1; last;
      }
    }
    if (!$gotone) {
      die "Cannot find compiler spec file '$compiler_spec_file'\n";
    }

    &usage() if ($opt_help || @ARGV);

    &find_testsuites(); 

    $ENV{UPCC_NORC} = "1" if $norc;

    if (!defined($sysconf_file)) {
	$sysconf_file = $compiler_spec{default_sysconf};
	printf("HARNESSWARNING: -sysconf option missing, assuming -sysconf=$sysconf_file\n");
    }

    $sysconf = &parse_sysconfig($sysconf_file);
    $sysconf->{repository} = $repoacct if defined($repoacct);
    $sysconf->{nthread_default} = $upcthread if defined($upcthread);
    $sysconf->{max_proc_per_node} = $ppn if defined($ppn);
    $sysconf->{num_pthreads} = $npthread if defined($npthread);
    $sysconf->{network} = $conduit if defined($conduit);
    $sysconf->{shell_cmd} = $shell_cmd if defined($shell_cmd);
    $sysconf->{submit_threshold} = $submit_threshold if defined($submit_threshold);

    $compiler_featurelist = $compiler_spec{feature_list} if (!defined($compiler_featurelist));
    @compiler_features = map { s/^\s+//g; s/\s+$//g; lc($_) } split(/[, ]/,$compiler_featurelist);
    push @compiler_features, ($sysconf->{num_pthreads} ? "" : "no" ) . "pthreads" ;
    push @compiler_features, "conduit_".$sysconf->{network};
    if ($ENV{'PARSEQ'}) {
      if ($ENV{'PARSEQ'} =~ /par/) {
        push @compiler_features, "pthread_support";
      } else {
        push @compiler_features, "nopthread_support";
      }
    }
    push @compiler_features, "trans_profile" if ($upcc_profile || $upcc_profile_local);
    push @compiler_features, "trans_g" if ($upcc_debug || $upcc_tv);
    push @compiler_features, "trans_O" if $optimize;
    push @compiler_features, "trans_opt" if $trans_opt;
    push @compiler_features, "all";

    # allow command line options to re-define config file definitions
    @suite = split(/,/,join(',',@suite));
    @test = split(/,/,join(',',@test));
    foreach my $thistest (@test) {
	if ($thistest =~ m/(.*)\/([^\/]+)$/) { # contains a suite spec, select the suite
	  push @suite, $1;
	}
    }
    $sysconf->{testfilter} = \@test if (@test);
    if (@suite == 0) {
	# no suites specified thus far, run them all
	@suite = @ALL_TESTSUITES;
    }
    # select set of testsuites based on user input.
	my @newsuite = ();
	my $pat;
	foreach $pat (@suite) {
	    # find all known testsuites that match
	    # this pattern
	    my @lst = grep(/$pat$/,@ALL_TESTSUITES);
	    if (@lst > 0) {
		foreach my $entry (@lst) {
		  push(@newsuite,$entry) if (!grep(/^$entry$/,@newsuite));
                }
	    } else {
		&fatal("unknown testsuite pattern [$pat]");
	    }
	}
	$sysconf->{testsuites} = \@newsuite;

    if ($debug) {
	my $pat;
	my $ts = $sysconf->{testsuites};
	foreach $pat (@$ts) {
	    print "Selected Testsuite [$pat]\n";
	}
    }
    if (defined($lpath)) {
	$logdir = $lpath;
    } elsif (defined($sysconf->{logroot})) {
	$logdir = sprintf("%s/%s",$sysconf->{logroot},$timestamp);
    } else {
	$logdir = sprintf("%s/logroot/%s",$harness_bld_path,$timestamp);
    }
    # make sure logpath is an absolute path, since this program chdir's quite
    # promiscuously
    unless ($logdir =~ m@^/@) {
        $logdir = cwd() . "/$logdir";
    }

    if (defined($wpath)) {
	$top_work_path = $wpath;
    } elsif (defined($sysconf->{buildroot})) {
	$top_work_path = sprintf("%s/%s",$sysconf->{logroot},$timestamp);
    } else {
	$top_work_path = "$harness_bld_path/..";
    }
    chdir($top_work_path) or die "Can't cd to '$top_work_path': $!\n";
    $top_work_path = getcwd(); # use absolute path
    chdir($startdir) or die "Can't cd to '$startdir': $!\n";

    if ($sysconf->{network} eq "smp" && $sysconf->{num_pthreads} > 0 && 
        $sysconf->{nthread_default} != $sysconf->{num_pthreads}) {
	$sysconf->{nthread_default} = $sysconf->{num_pthreads};
        &logit("Set nthread_default to $sysconf->{nthread_default} to match num_pthreads for smp");
    }
    if ($sysconf->{network} eq "smp" && $sysconf->{num_pthreads} == 0 && 
        $sysconf->{nthread_default} != 1) {
	$sysconf->{nthread_default} = 1;
        &logit("Set nthread_default to 1 to match non-pthreaded smp");
    }

    $sysconf->{max_nodes_to_run} = $max_nodes if defined($max_nodes);
    $recompile = !$nocompile && !$dryrun;
    $runjobs = !$norun && !$dryrun;

    @includefilter = map { lc($_); } split(/,/,join(',',@includefilter));
    foreach my $filter (@includefilter) {
        my $tmp = $filter;
        $tmp =~ s/^(known-[a-z-]+)\+$/$1/; # strip trailing '+'
        $tmp =~ s/(\W)/\\$1/g; # quote non-alphanumeric to supress metachars in grep()
        unless (grep(/^$tmp$/,@VALID_FILTERS) || ($filter =~ m/^(require|prohibit)-feature:.+/)) {
            &fatal("Error: unknown -include filter [$filter]\n".
                   "Valid filters are: ".join(' ',@VALID_FILTERS)." require-feature:FEATURE and prohibit-feature:FEATURE");
        }
    }
    $sysconf->{includefilter} = \@includefilter if (@includefilter);
    @excludefilter = map { lc($_); } split(/,/,join(',',@excludefilter));
    foreach my $filter (@excludefilter) {
        my $tmp = $filter;
        $tmp =~ s/^(known-[a-z-]+)\+$/$1/; # strip trailing '+'
        $tmp =~ s/(\W)/\\$1/g; # quote non-alphanumeric to supress metachars in grep()
        unless (grep(/^$tmp$/,@VALID_FILTERS) || ($filter =~ m/^(require|prohibit)-feature:.+/)) {
            &fatal("Error: unknown -exclude filter [$filter]\n".
                   "Valid filters are: ".join(' ',@VALID_FILTERS)." require-feature:FEATURE and prohibit-feature:FEATURE");
        }
    }
    $sysconf->{excludefilter} = \@excludefilter if (@excludefilter);
}

# ===============================================================
# print the usage command
# ===============================================================
sub usage
{
    print <<EOF;
Usage: harness [options]

This script will automatically compile and run all the tests within a
collection of UPC test suites.  It assumes two directory trees, the
source tree and the build tree.  The source tree is read-only and must
contain the upcr and gasnet sources as checked out from CVS.  The build
tree is where all the work will take place.  It should be placed in a
large file system because this script has the potential to compile hundreds 
of UPC examples.  The build tree should be configured
as follows:
      cd SOURCE_TREE_ROOT
      ./Bootstrap
      cd BUILD_TREE_ROOT
      SOURCE_TREE_ROOT/configure <configuration options>
      make
      cd BUILD_TREE_ROOT/harness
      ./harness <options... see below>

Note: the configuration process will automatically set the values of
SOURCE_TREE_ROOT and BUILD_TREE_ROOT.

The harness will cycle through the list of selected test suites and 
perform the following actions:
     - copy SOURCE_TREE_ROOT/<suite> to BUILD_TREE_ROOT/<suite>
     - cd BUILD_TREE_ROOT/<suite>
     - read the harness.conf file for the suite
     - compile each test in the suite according to its specification
       in the harness.conf file.
     - If successful, arrange for the test to be executed on the host system.
     
The queue scripts, run logs and text reports will be found in the 
BUILD_TREE_ROOT/harness/logroot/YYYYMMDD_HHMMSS directory.


Here are the harness command line options (default options may be set 
using environment variable \$HARNESS_FLAGS):
   -help              Print this usage menu.
   -nocompile         Do not compile the test suite.
   -norun             Generate the run scripts, but do not submit them for execution.
   -dryrun            Parse the harness.conf for the selected suite(s), but don't
                         actually compile or run any tests.  This is an alias for
                         -nocompile -norun.  This flag does not supress the copy of
                         the test suite.
   -compileonly       Only copy and compile the test suite - deletes the generated
                         binaries and does not generate or submit run scripts.
   -sysconf=file      Specify the system configuration file.
   -threads=N         Specify default number of UPC threads.
   -pthreads=N        Specify number of pthreads per process.
   -ppn=N             Specify max number of processes per node.
   -network=s         Specify the network [GASNet conduit].
   -repo=name         Specify name of the accounting repository (Seaborg).
   -filepat=string    Specify a filename pattern.  Only tests in the
                         suite(s) that match this pattern will be compiled/run.
   -logdir=path       Optionally specify the location of the logging directory.  
                         If not specified, the harness will create a date and
                         timestamped directory within the build tree as follows:
                         BUILD_TREE_ROOT/harness/logroot/YYYYMMDD_HHMMSS.
                         This is where the logs, the batch queue scripts and
                         the compile and run reports will be placed.
   -workdir=path      Optionally specify the location of the working directory.  
                         If not specified, the harness will use BUILD_TREE_ROOT.
                         The given directory must exist.
   -norc              Pass '-norc' to upcc compiler.
   -runlimit=N        Default runtime limit (in seconds) for each test
   -timeout_multiplier=N.M  Multiplier to adjust all time limits based on system 
                      performance and load. Use values > 1.0 to compensate for  
                      slow systems that encounter false negatives due to timeouts.
   -submit_threshold=N   If non-zero, queue scripts will be submitted for execution
                         when their accumulated worst-case running time approaches
                         this threshold (when adding the next job would exceed it).
                         This results in submitting more shorter-running jobs, than
                         not using this option.
                         For systems where compilation is slow and queue wait times
                         are short, this can dramatically improve time to completion.
                         If you cannot (or must not) submit batch jobs from inside
                         a batch job, use this option to ensure each batch job is
                         fully self-contained.
   -clean             Remove and re-copy build directory testsuite before running
   -keep              Do not remove the app binary after the test run
                      -nokeep will always remove the app binary after the test run
                         By default, the runjobs script will remove the
                         application binary if and only if the test passes.
   -compiler_spec=file Compiler spec file containing parameters defining how to
                      run the UPC compiler, defaults to $compiler_spec_file
   -upc_home=path     Override the upc_home variable in the compiler_spec file with given path.
   -features=str      Comma-separated list of features supported by compilation environment
                      Set automatically when running within upcr
   -g,-O,-opt,-tv,-profile,-profile-local                 
                      Pass the given flags to upcc for all tests.
   -debug             Turn on debugging flag.
   -max_node_run=num  Generate the runlist, but do not submit the job to
                         run if it requires more than this number of nodes.
   -include=string    Specify a condition that a test must satisfy if it is
                      to be compiled/run.  Pass an invalid condition (such as
                      "help") for a list of valid conditions.  The "known-*"
                      conditions normally operate on KnownFailure conditions
                      that apply to the current features list, but may be
                      suffixed with a '+' to ignore the features list.
                      NOTE: the argument may be a comma seperated list or
                      this option may be repeated.
   -exclude=string    Like -include, except that the conditions must NOT be
                      satisfied if the test is to be compiled/run.
   -test=name         Specify the name of particular test(s) to run
                      Comma-separated list of tests to run, optionally preceded
                      by the suite name. For example, -test=bug53,benchmarks/barrierperf
                      Default is to run all the tests in the suites selected by -suite.
   -suite=name        Specify the name of test suite(s) to run.  You need only
                      specify the unique trailing part of the name.
                      For example, -suite=cpi,gwu would select upc-examples/cpi
                      and upc-tests/gwu.  NOTE: the argument may be a comma 
                      seperated list or this option may be repeated.
                      The default is to run all the test suites.
                      The current set of suites to choose from is:
EOF
    &find_testsuites(); 
    my @display_suites = @ALL_TESTSUITES;
    foreach my $suite_dir (split(':', $compiler_spec{suite_path})) {
	$suite_dir =~ s/\/*$//;
	$suite_dir = dirname($suite_dir);
	map { s/^$suite_dir\///; } @display_suites;
    }
    foreach my $test (sort @display_suites) {
	print("\t\t\t\t$test\n");
    }
    exit(0);
}

