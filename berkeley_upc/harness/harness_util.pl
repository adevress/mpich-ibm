#!/usr/bin/env perl

require 5.005;
use strict;
use POSIX ":sys_wait_h";

my $debug = 0;

# is_cygwin() returns true iff this is a cygwin platform
# is_unicos() returns true iff this is a unicos platform
{
 my $is_cygwin_result = undef;
 sub is_cygwin() {
  if (!defined $is_cygwin_result) {
    my $sysname = `uname`;
    chomp($sysname);
    $is_cygwin_result = ($sysname =~ /cygwin/i) ? 1 : 0;
  }
  return $is_cygwin_result;
 }
 my $is_unicos_result = undef;
 sub is_unicos() {
  if (!defined $is_unicos_result) {
    my $sysname = `uname`;
    chomp($sysname);
    $is_unicos_result = ($sysname =~ /UNICOS/i) ? 1 : 0;
  }
  return $is_unicos_result;
 }
}

# get_tmpfilename() returns a unique name of a tmpfile 
{
  my $TMPDIR = (-d $ENV{TMPDIR} ? "$ENV{TMPDIR}" : '/tmp');
  my $USER = ( $ENV{USER} || "x" );
  my $tmpfile_id = 0;
  sub get_tmpfilename() {
    $tmpfile_id++;
    return "$TMPDIR/harness-$USER-$$-$tmpfile_id";
  }
}

# exec a command string, redirect stdout and stderr, and try to preserve signal forwarding
sub execcmd($$$) {
  my $cmd = shift;
  my $out = shift;
  my $err = shift;
  if (is_cygwin()) {
    # try backticks so shell can handle redirection
    $_ = `$cmd >$out 2>$err`;
    exit ($? >> 8); 
  } else {
    # Go to some trouble to avoid spawning a subshell which might
    # mess with signalling.
    my @cmdlist;
    # be sure to honor quoting of spaces in command
    # this doesn't implement the full shell quoting rules, but it's probably close enough 
    my $quot = undef;
    my $arg = undef;
    foreach my $char (split //, $cmd) {
      if ($char =~ /['"]/ && !$quot) { # open quote
        $quot = $char;
        $arg .= ""; # ensure non-empty
      } elsif ($char eq $quot) { # close quote
        $quot = undef;
      } elsif ($char =~ /[ \n\t]/ && !$quot) {
        push @cmdlist, $arg if (defined $arg);
        $arg = undef;
      } else {
        $arg .= $char;
      }
    }
    push @cmdlist, $arg if (defined $arg);
    #print "EXECARGS: " .(join "|", @cmdlist)."\n";
    open STDOUT, ">$out"
                or die "unable to redirect stdout to $out: $!\n";
    open STDERR, ">$err"
                or die "unable to redirect stderr to $err: $!\n";
    exec(@cmdlist);
    die "exec failed";
  }
}


my $got_timeout = 0;
my $child_pid = undef;

sub runjob_withtimeout($$) {
    my $childfn = shift;
    my $timeout = shift;
    # set the signal handler to catch an alarm
    $SIG{'ALRM'} = 'catch_alarm';

    $got_timeout = 0;
    $child_pid = undef;
    $child_pid = fork();

    if (defined($child_pid) && ($child_pid == 0)) {
        # we are the child
        eval { setpgrp(0,0); }; # try to create a new process group to assist signalling
	&$childfn();
        exit $?;
    } elsif (!defined($child_pid) || ($child_pid < 0)) {
	# fork failed... bail
	die "fork failed";
    }

    #if we got here, we are the parent
    # timeout must be a positive integer - round it up to conform
    $timeout++ if ($timeout != int($timeout));
    $timeout = int($timeout);
    $timeout = 1 if ($timeout < 1);
    alarm $timeout;

    # wait for the job to complete
    waitpid($child_pid,0);

    # recover the exit code of the child process
    my $status = $?;
    if (is_unicos()) { $status = ($status >> 8); } # bug 1835
    else { $status = (($status >> 8) || ($status & 127)); }

    # disable the alarm if it has not gone off
    alarm 0;

    return ($status,$got_timeout);
}

# ======================================================================
# The child killer!
# Terminate a child process
#
# At least one mpirun (LAM) is known to leave orphans if it gets
# SIGTERM.  So, we try sending SIGINT first.
# ======================================================================
sub kill_child {
    my @signals = qw/INT TERM/;
    if (is_cygwin()) {
	# I don't know why, but it prevents runaways -PHH
	unshift @signals, 'USR1';
    }
    my $signals = @signals;
    my $limit = $signals*2;

    sub alive {
        my $pid = waitpid($child_pid, &WNOHANG);
	if ($pid == 0) {
	    # the child is still running
	    return 1;
	} elsif (($pid == -1) || WIFEXITED($?) || WIFSIGNALED($?)) {
	    # the child has exited
	    return 0;
	} else {
	    # the child is stopped?
	    kill 'CONT', $child_pid;
	    return 0;
	}
    }

    my $count = 0;
    my $grpid = undef;
    eval { $grpid = getpgrp($child_pid); # lookup child process group
           $grpid = undef if ($grpid == getpgrp(0)); # be careful not to kill ourselves
         };

    while (&alive && ($count < $limit)) {
        my $sig = $signals[$count % $signals];
	if ($grpid) { # give preference to a process group kill, 
                      # which is less likely to leave zombies due to incorrect signal propagation
	  print "SENDING: kill $sig, -$grpid\n" if ($debug);
	  kill $sig, -$grpid || 
	    print "WARNING: failed to kill $sig -$grpid: $!\n";
	  sleep 15;
          last if (!&alive);
        }
	print "SENDING: kill $sig, $child_pid\n" if ($debug);
	kill $sig, $child_pid || 
	  print "WARNING: failed to kill $sig $child_pid: $!\n";
	sleep 15;
	++$count;
    }

    # Give up on being polite
    if (&alive) {
      print "SENDING: kill KILL, $child_pid\n" if ($debug);
      kill 'KILL', $child_pid || 
	  print "WARNING: failed to kill KILL $child_pid: $!\n";
    }
}


# ======================================================================
# The signal handler for an alarm.
# Record the fact that the alarm went off and kill the child process.
# ======================================================================
sub catch_alarm {
    return if ($child_pid <= 0);
    $got_timeout = 1;
    kill_child();
}

1;
