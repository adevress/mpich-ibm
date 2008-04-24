#!/usr/bin/perl

=head1 COPYRIGHT

Product(s):
5733-BG1

(C)Copyright IBM Corp. 2004, 2004
All rights reserved.
US Government Users Restricted Rights -
Use, duplication or disclosure restricted
by GSA ADP Schedule Contract with IBM Corp.

Licensed Materials-Property of IBM

=cut

my $COPYRIGHT = 'Licensed Materials - Property
 of IBM, 5733-BG1 (C) COPYRIGHT 2004, 2004
 All Rights Reserved. US Government Users restricted Rights -
 Use, Duplication or Disclosure restricted by GSA ADP Schedule
 Contract with IBM Corp.';


# This is a quick script to generate mpixl* from mpi{cc|cxx|f77} scripts and a single config file.
# Arguments are path-to-mpicc and path-to-this-script
#
# It is run in the MPI-level makefile
# smithbr: Do we need to change the text file stuff too maybe?

use File::Copy;

# print "Creating mpixl* compiler scripts\n";
$xlcomp_conf="mpixl.conf.in";
if($ARGV[0] eq "")
{
   print "Must provide location of existing bin/mpixxx and etc/mpixxx.conf\n";
   die;
}

if($ARGV[1] eq "")
{
   print "Must provide location of this script\n";
   die;
}

$prefix_path=$ARGV[0];
$script_path=$ARGV[1];
if ($ARGV[2] ne "") {
  $target=$ARGV[2];
}
else {
  $target="BGL";
}

if(! -e "$prefix_path/bin/mpicc")
{
   print "Must provide location of existing bin/mpixxx and etc/mpixxx.conf\n";
   die;
}

$mpicc="$prefix_path/bin/mpicc";
$mpif77="$prefix_path/bin/mpif77";
$mpicxx="$prefix_path/bin/mpicxx";

$mpixlc="$prefix_path/bin/mpixlc";
$mpixlf77="$prefix_path/bin/mpixlf77";
$mpixlcxx="$prefix_path/bin/mpixlcxx";
$mpixlf90="$prefix_path/bin/mpixlf90";
$mpixlf95="$prefix_path/bin/mpixlf95";
$mpixlf2003="$prefix_path/bin/mpixlf2003";

if ($target eq "BGP") {
    $mpixlc_r="$prefix_path/bin/mpixlc_r";
    $mpixlf77_r="$prefix_path/bin/mpixlf77_r";
    $mpixlcxx_r="$prefix_path/bin/mpixlcxx_r";
    $mpixlf90_r="$prefix_path/bin/mpixlf90_r";
    $mpixlf95_r="$prefix_path/bin/mpixlf95_r";
    $mpixlf2003_r="$prefix_path/bin/mpixlf2003_r";
}

# something to start with

copy("$mpicc", "$mpixlc");
copy("$mpicxx", "$mpixlcxx");
copy("$mpif77", "$mpixlf77");
copy("$mpif77", "$mpixlf90");
copy("$mpif77", "$mpixlf95");
copy("$mpif77", "$mpixlf2003");
chmod(0755,$mpixlc);
chmod(0755,$mpixlcxx);
chmod(0755,$mpixlf77);
chmod(0755,$mpixlf90);
chmod(0755,$mpixlf95);
chmod(0755,$mpixlf2003);

if ($target eq "BGP") {
    copy("$mpicc", "$mpixlc_r");
    copy("$mpicxx", "$mpixlcxx_r");
    copy("$mpif77", "$mpixlf77_r");
    copy("$mpif77", "$mpixlf90_r");
    copy("$mpif77", "$mpixlf95_r");
    copy("$mpif77", "$mpixlf2003_r");
    chmod(0755,$mpixlc_r);
    chmod(0755,$mpixlcxx_r);
    chmod(0755,$mpixlf77_r);
    chmod(0755,$mpixlf90_r);
    chmod(0755,$mpixlf95_r);
    chmod(0755,$mpixlf2003_r);
}

# make sure xlcomp file is in $script_path...
if(! -e "$script_path/$xlcomp_conf")
{
   print "Couldn't find $xlcomp_conf in $script_path\n";
   die;
}


# read in config variables
open CONF,"<$script_path/$xlcomp_conf" || die;
while(<CONF>)
{
   if(/^(\w+)=(.*)/)
   {

# save the right xl compiler path, depending on whether it is BGP or BGL

     $vars{$1}=$2;
   }
}
close CONF;

# start replacing variables in the mpixl* scripts with variables read in from $xlcomp
open MPIXLC,">$mpixlc" || die;
if ($target eq "BGP") {
    open MPIXLCR,">$mpixlc_r" || die;
}
open MPICC,"<$mpicc" || die;
while(<MPICC>)
{
   if(/^CC=/ && defined($vars{'XL9C'}))
   {
      $C9=$vars{'XL9C'};
      if ($target eq "BGL") {
         $_="if [ -e $C9 ]; then\n";
         print MPIXLC $_;
         $_="CC=$C9\n";
         print MPIXLC $_;
         $_="else\n";
         print MPIXLC $_;
         $_="CC=$vars{'XL8C'}\n";
         print MPIXLC $_;
         $_="fi\n";
         print MPIXLC $_;
       }
       else {
         $_="CC=$C9\n";
         print MPIXLC $_;
         if (defined(MPIXLCR)) {
	    $_="CC=$vars{'XL9CR'}\n";
	    print MPIXLCR $_;
         }
      }
   }
   else {
       if(/^MPI_OTHERLIBS=/ && defined($vars{'MPI_OTHERLIBS'})) {
	    $_="MPI_OTHERLIBS=$vars{'MPI_OTHERLIBS'}\n";
   	}
       elsif(/^MPI_CFLAGS=/&& defined($vars{'MPI_CFLAGS'})) {
	   $_="MPI_CFLAGS=$vars{'MPI_CFLAGS'}\n";
       }
       elsif(/^MPI_LDFLAGS=/ && defined($vars{'MPI_LDFLAGS'})) {
	   $_="MPI_LDFLAGS=$vars{'MPI_LDFLAGS'}\n";
       }
       print MPIXLC $_;
       if (defined(MPIXLCR)) {
	   print MPIXLCR $_;
       }
   }
}
close MPIXLC;
if (defined(MPIXLCR)) {
    close MPIXLCR;
}
close MPICC;

open MPIXLCXX,">$mpixlcxx" || die;
if ($target eq "BGP") {
    open MPIXLCXXR, ">$mpixlcxx_r" || die;
}
open MPICXX,"<$mpicxx" || die;
while(<MPICXX>)
{
   if(/^CXX=/ && defined($vars{'XL9CXX'}))
   {
      $C9XX=$vars{'XL9CXX'};
      if ($target eq "BGL") {
          $_="if [ -e $C9XX ]; then\n";
          print MPIXLCXX $_;
          $_="CXX=$C9XX\n";
          print MPIXLCXX $_;
          $_="else\n";
          print MPIXLCXX $_;
          $_="CXX=$vars{'XL8CXX'}\n";
          print MPIXLCXX $_;
          $_="fi\n";
          print MPIXLCXX $_;
      }
      else {
          $_="CXX=$C9XX\n";
          print MPIXLCXX $_;
      }
      if (defined(MPIXLCXXR)) {
	  $_="CXX=$vars{'XL9CXXR'}\n";
	  print MPIXLCXXR $_;
      }
   }
   else {
       if(/^MPI_OTHERLIBS=/ && defined($vars{'MPI_OTHERLIBS'})) {
	   $_="MPI_OTHERLIBS=$vars{'MPI_OTHERLIBS'}\n";
       }
       elsif(/^MPI_CXXFLAGS=/ && defined($vars{'MPI_CXXFLAGS'})) {
	   $_="MPI_CXXFLAGS=$vars{'MPI_CXXFLAGS'}\n";
       }
       elsif(/^MPI_LDFLAGS=/ && defined($vars{'MPI_LDFLAGS'})) {
	   $_="MPI_LDFLAGS=$vars{'MPI_LDFLAGS'}\n";
       }
       print MPIXLCXX $_;
       if (defined(MPIXLCXXR)) {
	   print MPIXLCXXR $_;
       }
   }

}
close MPIXLCXX;
if (defined(MPIXLCXXR)) {
    close MPIXLCXXR;
}
close MPICXX;

open MPIXLF77,">$mpixlf77" || die;
open MPIXLF90, ">$mpixlf90" || die;
open MPIXLF95, ">$mpixlf95" || die;
open MPIXLF2003, ">$mpixlf2003" || die;
if ($target eq "BGP") {
    open MPIXLF77R,">$mpixlf77_r" || die;
    open MPIXLF90R, ">$mpixlf90_r" || die;
    open MPIXLF95R, ">$mpixlf95_r" || die;
    open MPIXLF2003R, ">$mpixlf2003_r" || die;
}

open MPIF77,"<$mpif77" || die;
while(<MPIF77>)
{
   if (/^F77=/) {
     $FCOMP='F77';
   }
   elsif (/^FC=/) {
     $FCOMP='FC';
   }
   else {
     $FCOMP='';
   }
   if ($FCOMP ne '') {
      $F11=$vars{'XL11F77'};
      if ($target eq "BGL") {
          $_="if [ -e $F11 ]; then\n";
          print MPIXLF77 $_;
          $_="$FCOMP=$F11\n";
          print MPIXLF77 $_;
          $_="else\n";
          print MPIXLF77 $_;
          $_="$FCOMP=$vars{'XL10F77'}\n";
          print MPIXLF77 $_;
          $_="fi\n";
          print MPIXLF77 $_;
      }
      else {
          $_="$FCOMP=$vars{'XL11F77'}\n";
          print MPIXLF77 $_;
          if (defined('XL11F77R')) {
              $_="$FCOMP=$vars{'XL11F77R'}\n";
              print MPIXLF77R $_;
          }
      }
      $F11=$vars{'XL11F90'};
      if ($target eq "BGL") {
         $_="if [ -e $F11 ]; then\n";
         print MPIXLF90 $_;
         $_="$FCOMP=$F11\n";
         print MPIXLF90 $_;
         $_="else\n";
         print MPIXLF90 $_;
         $_="$FCOMP=$vars{'XL10F90'}\n";
         print MPIXLF90 $_;
         $_="fi\n";
         print MPIXLF90 $_;
      }
      else {
         $_="$FCOMP=$F11\n";
         print MPIXLF90 $_;
         if (defined($vars{'XL11F90R'})) {
           $_="$FCOMP=$vars{'XL11F90R'}\n";
           print MPIXLF90R $_;
         }
      }
      $F11=$vars{'XL11F95'};
      if ($target eq "BGL") {
         $_="if [ -e $F11 ]; then\n";
         print MPIXLF95 $_;
         $_="$FCOMP=$F11\n";
         print MPIXLF95 $_;
         $_="else\n";
         print MPIXLF95 $_;
         $_="$FCOMP=$vars{'XL10F95'}\n";
         print MPIXLF95 $_;
         $_="fi\n";
         print MPIXLF95 $_;
      }
      else {
         $_="$FCOMP=$vars{'XL11F95'}\n";
         print MPIXLF95 $_;
         if (defined($vars{'XL11F95R'})) {
            $_="$FCOMP=$vars{'XL11F95R'}\n";
            print MPIXLF95R $_;
         }
      }
      if (defined($vars{'XL11F2003'})) {
         $_="$FCOMP=$vars{'XL11F2003'}\n";
         print MPIXLF2003 $_;
         if (defined($vars{'XL11F2003R'})) {
            $_="$FCOMP=$vars{'XL11F2003R'}\n";
            print MPIXLF2003R $_;
         }
      }
    }
   else {
       if(/^MPI_OTHERLIBS=/ && defined($vars{'MPI_OTHERLIBS'})) {
           $_="MPI_OTHERLIBS=$vars{'MPI_OTHERLIBS'}\n";
       }
       elsif(/^MPI_LDFLAGS=/ && defined($vars{'MPI_LDFLAGS'})) {
           $_="MPI_LDFLAGS=$vars{'MPI_LDFLAGS'}\n";
       }
       elsif(/^F77CPP=/ && defined($vars{'F77CPP'})) {
           $_="F77CPP=$vars{'F77CPP'}\n";
       }

       elsif(/^MPI_FFLAGS=/ && defined($vars{'MPI_FFLAGS'})) {
	   $_="MPI_FFLAGS=$vars{'MPI_FFLAGS'}\n";
       }
       s/ -fno-underscoring//g;

       print MPIXLF77 $_;
       print MPIXLF90 $_;
       print MPIXLF95 $_;
       print MPIXLF2003 $_;

       if ($target eq "BGP") {
	   print MPIXLF77R $_;
	   print MPIXLF90R $_;
	   print MPIXLF95R $_;
	   print MPIXLF2003R $_;
       }
   }
}
close MPIXLF77;
close MPIXLF90;
close MPIXLF95;
close MPIXLF2003;

if ($target eq "BGP") {
    close MPIXLF77R;
    close MPIXLF90R;
    close MPIXLF95R;
    close MPIXLF2003R;
}
close MPIF77;

# print "Done\n";
