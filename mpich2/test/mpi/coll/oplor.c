/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *
 *  (C) 2003 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#include "mpi.h"
#include "mpitestconf.h"
#include <stdio.h>
#include "mpitest.h"

static char MTEST_Descrip[] = "Test MPI_LOR operations on optional datatypes dupported by MPICH2";

/*
 * This test looks at the handling of logical and for types that are not 
 * integers or are not required integers (e.g., long long).  MPICH2 allows
 * these as well.  A strict MPI test should not include this test.
 */
int main( int argc, char *argv[] )
{
    int errs = 0, err;
    int rank, size;
    MPI_Comm      comm;
    char cinbuf[3], coutbuf[3];
    signed char scinbuf[3], scoutbuf[3];
    unsigned char ucinbuf[3], ucoutbuf[3];
    float finbuf[3], foutbuf[3];
    double dinbuf[3], doutbuf[3];

    MTest_Init( &argc, &argv );

    comm = MPI_COMM_WORLD;

    MPI_Comm_rank( comm, &rank );
    MPI_Comm_size( comm, &size );

    /* Some MPI implementations do not implement all of the required
       (datatype,operations) combinations, and further, they do not
       always provide clear and specific error messages.  By catching 
       the error, we can provide a higher quality, more specific message.
    */
    MPI_Comm_set_errhandler( comm, MPI_ERRORS_RETURN );

#ifndef USE_STRICT_MPI
    /* char */
    MTestPrintfMsg( 10, "Reduce of MPI_CHAR\n" );
    cinbuf[0] = 1;
    cinbuf[1] = 0;
    cinbuf[2] = (rank > 0);

    coutbuf[0] = 0;
    coutbuf[1] = 1;
    coutbuf[2] = 1;
    err = MPI_Reduce( cinbuf, coutbuf, 3, MPI_CHAR, MPI_LOR, 0, comm );
    if (err) {
	errs++;
	MTestPrintError( err );
    }
    else {
	if (rank == 0) {
	    if (!coutbuf[0]) {
		errs++;
		fprintf( stderr, "char OR(1) test failed\n" );
	    }
	    if (coutbuf[1]) {
		errs++;
		fprintf( stderr, "char OR(0) test failed\n" );
	    }
	    if (!coutbuf[2] && size > 1) {
		errs++;
		fprintf( stderr, "char OR(>) test failed\n" );
	    }
	}
    }
#endif /* USE_STRICT_MPI */

    /* signed char */
    MTestPrintfMsg( 10, "Reduce of MPI_SIGNED_CHAR\n" );
    scinbuf[0] = 1;
    scinbuf[1] = 0;
    scinbuf[2] = (rank > 0);

    scoutbuf[0] = 0;
    scoutbuf[1] = 1;
    scoutbuf[2] = 1;
    err = MPI_Reduce( scinbuf, scoutbuf, 3, MPI_SIGNED_CHAR, MPI_LOR, 0, comm );
    if (err) {
	errs++;
	MTestPrintError( err );
    }
    else {
	if (rank == 0) {
	    if (!scoutbuf[0]) {
		errs++;
		fprintf( stderr, "signed char OR(1) test failed\n" );
	    }
	    if (scoutbuf[1]) {
		errs++;
		fprintf( stderr, "signed char OR(0) test failed\n" );
	    }
	    if (!scoutbuf[2] && size > 1) {
		errs++;
		fprintf( stderr, "signed char OR(>) test failed\n" );
	    }
	}
    }

    /* unsigned char */
    MTestPrintfMsg( 10, "Reduce of MPI_UNSIGNED_CHAR\n" );
    ucinbuf[0] = 1;
    ucinbuf[1] = 0;
    ucinbuf[2] = (rank > 0);

    ucoutbuf[0] = 0;
    ucoutbuf[1] = 1;
    ucoutbuf[2] = 1;
    err = MPI_Reduce( ucinbuf, ucoutbuf, 3, MPI_UNSIGNED_CHAR, MPI_LOR, 0, comm );
    if (err) {
	errs++;
	MTestPrintError( err );
    }
    else {
	if (rank == 0) {
	    if (!ucoutbuf[0]) {
		errs++;
		fprintf( stderr, "unsigned char OR(1) test failed\n" );
	    }
	    if (ucoutbuf[1]) {
		errs++;
		fprintf( stderr, "unsigned char OR(0) test failed\n" );
	    }
	    if (!ucoutbuf[2] && size > 1) {
		errs++;
		fprintf( stderr, "unsigned char OR(>) test failed\n" );
	    }
	}
    }

#ifndef USE_STRICT_MPI
    /* float */
    MTestPrintfMsg( 10, "Reduce of MPI_FLOAT\n" );
    finbuf[0] = 1;
    finbuf[1] = 0;
    finbuf[2] = (rank > 0);

    foutbuf[0] = 0;
    foutbuf[1] = 1;
    foutbuf[2] = 1;
    err = MPI_Reduce( finbuf, foutbuf, 3, MPI_FLOAT, MPI_LOR, 0, comm );
    if (err) {
	errs++;
	MTestPrintError( err );
    }
    else {
	if (rank == 0) {
	    if (!foutbuf[0]) {
		errs++;
		fprintf( stderr, "float OR(1) test failed\n" );
	    }
	    if (foutbuf[1]) {
		errs++;
		fprintf( stderr, "float OR(0) test failed\n" );
	    }
	    if (!foutbuf[2] && size > 1) {
		errs++;
		fprintf( stderr, "float OR(>) test failed\n" );
	    }
	}
    }

    /* double */
    MTestPrintfMsg( 10, "Reduce of MPI_DOUBLE\n" );
    dinbuf[0] = 1;
    dinbuf[1] = 0;
    dinbuf[2] = (rank > 0);

    doutbuf[0] = 0;
    doutbuf[1] = 1;
    doutbuf[2] = 1;
    err = MPI_Reduce( dinbuf, doutbuf, 3, MPI_DOUBLE, MPI_LOR, 0, comm );
    if (err) {
	errs++;
	MTestPrintError( err );
    }
    else {
	if (rank == 0) {
	    if (!doutbuf[0]) {
		errs++;
		fprintf( stderr, "double OR(1) test failed\n" );
	    }
	    if (doutbuf[1]) {
		errs++;
		fprintf( stderr, "double OR(0) test failed\n" );
	    }
	    if (!doutbuf[2] && size > 1) {
		errs++;
		fprintf( stderr, "double OR(>) test failed\n" );
	    }
	}
    }

#ifdef HAVE_LONG_DOUBLE
    { long double ldinbuf[3], ldoutbuf[3];
    /* long double */
    ldinbuf[0] = 1;
    ldinbuf[1] = 0;
    ldinbuf[2] = (rank > 0);

    ldoutbuf[0] = 0;
    ldoutbuf[1] = 1;
    ldoutbuf[2] = 1;
    if (MPI_LONG_DOUBLE != MPI_DATATYPE_NULL) {
	MTestPrintfMsg( 10, "Reduce of MPI_LONG_DOUBLE\n" );
	err = MPI_Reduce( ldinbuf, ldoutbuf, 3, MPI_LONG_DOUBLE, MPI_LOR, 0, comm );
	if (err) {
	    errs++;
	    MTestPrintError( err );
	}
	else {
	    if (rank == 0) {
		if (!ldoutbuf[0]) {
		    errs++;
		    fprintf( stderr, "long double OR(1) test failed\n" );
		}
		if (ldoutbuf[1]) {
		    errs++;
		    fprintf( stderr, "long double OR(0) test failed\n" );
		}
		if (!ldoutbuf[2] && size > 1) {
		    errs++;
		    fprintf( stderr, "long double OR(>) test failed\n" );
		}
	    }
	}
	}
    }
#endif /* HAVE_LONG_DOUBLE */
#endif /* USE_STRICT_MPI */

#ifdef HAVE_LONG_LONG
    {
	long long llinbuf[3], lloutbuf[3];
    /* long long */
    llinbuf[0] = 1;
    llinbuf[1] = 0;
    llinbuf[2] = (rank > 0);

    lloutbuf[0] = 0;
    lloutbuf[1] = 1;
    lloutbuf[2] = 1;
    if (MPI_LONG_LONG != MPI_DATATYPE_NULL) {
	MTestPrintfMsg( 10, "Reduce of MPI_LONG_LONG\n" );
	err = MPI_Reduce( llinbuf, lloutbuf, 3, MPI_LONG_LONG, MPI_LOR, 0, comm );
	if (err) {
	    errs++;
	    MTestPrintError( err );
	}
	else {
	    if (rank == 0) {
		if (!lloutbuf[0]) {
		    errs++;
		    fprintf( stderr, "long long OR(1) test failed\n" );
		}
		if (lloutbuf[1]) {
		    errs++;
		    fprintf( stderr, "long long OR(0) test failed\n" );
		}
		if (!lloutbuf[2] && size > 1) {
		    errs++;
		    fprintf( stderr, "long long OR(>) test failed\n" );
		}
	    }
	}
    }
    }
#endif

    MTest_Finalize( errs );
    MPI_Finalize();
    return 0;
}
