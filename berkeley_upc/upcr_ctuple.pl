################################################################################
# ctuple.pl
#
# Support functions for configuration tuple checking (and sizes info, too)
################################################################################

use strict;

# extract embedded keywords from binary UPC/Gasnet file
sub extract_ctuples
{
    my ($filename) = @_;
    my (%gasnet_ctuples, %upcr_ctuples, %upcr_sizes, %misc_info);
    
    open (FILE, $filename) or die "can't open file '$filename'\n";
    # use $ as the line break symbol, to make grepping for ident-style strings
    # simpler and more efficient.
    my $oldsep = $/;
    $/ = '$';
    while (<FILE>) {
        if (/^(GASNetConfig|UPCRConfig):            # $1: config type
                  \s+
                  \( ([^)]+) \)                     # $2: filename (in parens)
                  \s+
                  ([^\$]+?)                         # $3: config string
                  \ \$                              # space followed by $
            /x) 
        {
            if ($1 eq "GASNetConfig") {
                $gasnet_ctuples{$2} = $3;
            } elsif ($1 eq "UPCRConfig") {
                $upcr_ctuples{$2} = $3;
            } else {
                die "Internal regex error in extract_ctuples()\n";
            }
        } elsif (/^UPCRSizeof: \s+ 
                      ([A-Za-z0-9_]+)               # $1: type
                      =
                      ([\%-~])                      # char in %...~ range
                      \ \$
                 /x) {
                # subtract ASCII '$' from '%...~' to get 1...90 size
                $upcr_sizes{$1} = ord($2) - ord('$');
        } elsif (/^UPCRDefaultHeapSizes: \s+
                      UPC_SHARED_HEAP_OFFSET=([0-9]+[A-Za-z]*) 
                      \s+
                      UPC_SHARED_HEAP_SIZE=([0-9]+[A-Za-z]*) 
		      (?: \s+
                          UPC_SHARED_HEAP_SIZE_MAX=([0-9]+[A-Za-z]*) )?
                      \ \$
                 /x) {
                $upcr_sizes{UPC_SHARED_HEAP_OFFSET} = $1;
                $upcr_sizes{UPC_SHARED_HEAP_SIZE} = $2;
                $upcr_sizes{UPC_SHARED_HEAP_SIZE_MAX} = $3 if ($3);
        } elsif (/^UPCRDefaultPthreadCount: \s+
                  ([0-9]+)                          # $1: count
                  \ \$                              # space followed by $
		/x) {
		$misc_info{DefaultPthreadCount}{'<link>'} = $1;
        } elsif (/^(UPC\S+): \s+                    # $1: other misc UPCR per-file ident string
                  \( ([^)]+) \)                     # $2: filename (in parens)
                  \s+
                  ([^\$]+?)                         # $3: value
                  \ \$                              # space followed by $
		/x) {
		$misc_info{$1}{$2} = $3;
        } elsif (/^(UPCR\S+|GASNet\S+): \s+                   # $1: other misc UPCR ident string
                  ([^\$]+)                          # $2: value
                  \ \$				    # space followed by $
                /x) {
            $misc_info{$1} = $2;
        }

    }
    close (FILE);
    $/ = $oldsep;

    # return by ref to avoid flattening
    return (\%gasnet_ctuples, \%upcr_ctuples, \%upcr_sizes, \%misc_info); 
}

# check a .trans.c source file contains the proper ctuple strings
sub check_ctuple_trans

{
    my $filename = $_[0];
    open (TRANS_FILE, $filename) or die "could not read $filename: $!\n";
    my $oldsep = $/;
    undef $/;                # open maw
    my $transtxt = <TRANS_FILE>; # slurp!
    $/ = $oldsep;            # close maw
    close TRANS_FILE;

    unless (
     ($transtxt =~ m/UPCRI_IdentString_.*_GASNetConfig_gen/) &&
     ($transtxt =~ m/UPCRI_IdentString_.*_GASNetConfig_obj/) &&
     ($transtxt =~ m/UPCRI_IdentString_.*_UPCRConfig_gen/) &&
     ($transtxt =~ m/UPCRI_IdentString_.*_UPCRConfig_obj/)  
    ) { die "file $filename is missing mandatory configuration strings!\n"; }
}

# Check the consistency of a UPC object by comparing its configuration tuples,
# both internally and optionally with a canonical model
{
 my $mismatch_warned = 0;
 sub check_ctuple_obj {
    my ($filename, $allow_missing, $canon_gasnet, $canon_upcr) = @_;
    my ($gasnet_ctuples, $upcr_ctuples, $upcr_sizes, $misc_info) = extract_ctuples($filename);
    my @ctup = (%$gasnet_ctuples,%$upcr_ctuples);
    my $error;
    my $upofile = 1 unless $filename =~ m/.*_startup_tmp.o$/;
    if (@ctup == 0 && $allow_missing) {
        return 0;  # not a UPC object: presumably C object
    } elsif (($upofile && @ctup != 8) || (!$upofile && @ctup != 4)) {
        $error = "missing build config strings in '${filename}'\n";
    } elsif ($upofile && ($ctup[1] ne $ctup[3] || $ctup[5] ne $ctup[7])) {
        $error = "inconsistent build configuration in '${filename}':\n" .
               $ctup[0] . ":\n " . $ctup[1] . "\n" .
               $ctup[2] . ":\n " . $ctup[3] . "\n" .
               $ctup[4] . ":\n " . $ctup[5] . "\n" .
               $ctup[6] . ":\n " . $ctup[7] . "\n";
    } elsif ($canon_upcr && $ctup[@ctup - 1] ne $canon_upcr) {
        $error = "UPCR build configuration in '${filename}':\n" .
                 " " . $ctup[@ctup - 1] . "\n" .
                 "doesn't match link configuration:\n" .
                 " $canon_upcr\n";
    } elsif ($canon_gasnet && $ctup[1] ne $canon_gasnet) {
        $error = "GASNet build configuration in '${filename}':\n" .
                 " " . $ctup[1] . "\n" .
                 "doesn't match link configuration:\n" .
                 " $canon_gasnet\n";
    } 
    if (!defined $error && 
        $$misc_info{UPCRConfigureMismatch} && !$mismatch_warned &&
	$ctup[@ctup - 1] !~ /TRANS=gccupc/) { # bug 1853
       foreach my $filen (keys %{$$misc_info{UPCRConfigureMismatch}}) {
         my $comppath = $$misc_info{UPCRBackendCompiler}{$filen} || "*unknown path*";
         my $buildcomp = $$misc_info{UPCRBuildCompiler}{$filen} || "*unknown id*";
         my $confcomp = $$misc_info{UPCRConfigureCompiler}{$filen} || "*unknown id*";
	 $comppath = upcc_decode($comppath);
         print STDERR "upcc: warning: Configuration mismatch detected!\n".
                      " This install of Berkeley UPC was configured with backend C compiler '$comppath', which was identified as:\n".
	  	      "   $confcomp\n".
		      " However this C compiler now identifies as:\n".
		      "   $buildcomp\n".
		      " This usually indicates the C compiler was changed/upgraded and UPC was not reinstalled. ".
		    "Berkeley UPC is a source-to-source compilation system, and is therefore sensitive to details of the C compiler setup, even after installation. This configure/use mismatch is likely to cause correctness/performance problems - please re-configure and re-install Berkeley UPC with the new C compiler.\n";
         $mismatch_warned = 1; # warn at most once per compile
	 last;
       }
    }
    return $error;
 }
}

1;
