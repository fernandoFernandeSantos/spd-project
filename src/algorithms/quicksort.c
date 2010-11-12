
/**
 * @file quicksort.c
 *
 * @brief This file contains a parallel version of quicksort, a standard algorithm to sort atomic elements (integers) 
 *
 * To scatter data:
 *   each node will call \scatter, which takes an array \array of \size=M elements.
 *   when \scatter returns, only the first \size items of \array will be valid:
 *     \array will contain the partition each node must sort.
 * To gather back data:
 *   each node, but node 0, sends its sorted partition to node 0.
 *   node 0 will sequentially read data from any node, into the original array,
 *     (at the right offset) so that no merging will be required.
 *
 *
 * @author Paolo Giangrandi
 * @version 0.0.01
 */

#include "sorting.h"
#include "utils.h"
#include "string.h"

/**************************************/
/*               STENCIL              */
/**************************************/

#define ACTIVE_PROCS(ti,step) _pow2(step)
#define GET_STEP_COUNT(ti) _log2( GET_N(ti) )

//from_who	: return the rank of the process (node) from which i RECEIVE data in the current step
int from_who( const TestInfo *ti, int step ) 
{
	return GET_ID(ti) - ( GET_N(ti) / ACTIVE_PROCS(ti,step+1) );
}

//to_who	: return the rank of the process (node) from which i SEND data in the current step
int to_who( const TestInfo *ti, int step ) 
{
	return GET_ID(ti) + ( GET_N(ti) / ACTIVE_PROCS(ti,step+1) );
}

//do_i_send	: true if the calling process (node) has to SEND data TO another process (node) in the current step
bool do_i_send( const TestInfo *ti, int step )
{
	return ! ( GET_ID(ti) % ( GET_N(ti) / ACTIVE_PROCS(ti,step) ) );
}

//do_i_receive 	: return a number different from 0 if the calling process (node) has to RECEIVE data FROM another process (node) in the current step.
bool do_i_receive( const TestInfo *ti, int step )
{
	return ! do_i_send( ti, step ) && ! ( GET_ID(ti) % ( GET_N(ti) / ACTIVE_PROCS(ti,step+1) ) ) ;
}



/**************************************/
/*            ALGORITHMIC             */
/**************************************/

#define SWAP( a, i, j ) { int tmp; tmp = a[i]; a[i] = a[j]; a[j] = tmp; }

// partitions the array:
// a is the array, of size size. p is the pivot
long partition( int *a, long size, int p )
{
	long i = 0, j = size-1;
	
	while( true ) {
		while( a[i] < p ) { ++ i; }
		while( a[j] > p ) { -- j; }
		if( i >= j ) {
			break;
		}
		SWAP( a, i, j );
	}
	
	return j;
}

void scatter( const TestInfo *ti, int *a, long *size )
{
	int step = 0;
	
	for( step = 0; step < GET_STEP_COUNT(ti); ++ step ) {
		if( do_i_send(ti,step) ) {
			long lim = partition( a, *size, a[0] );
			*size = lim;
			
			_MPI_Send ( & a[lim], *size-lim, MPI_INT, to_who( ti, step ), 0, MPI_COMM_WORLD );
		}
		if( do_i_receive(ti,step) ) {
			MPI_Status stat;
			_MPI_Recv( a, GET_M(ti), MPI_INT, from_who( ti, step ), 0, MPI_COMM_WORLD, &stat );
			_MPI_Get_count( &stat, MPI_INT, size );
		}
	}
	
	printf( "Node %d has got %ld data\n", GET_ID(ti), *size );
	
}



/**************************************/
/*              SORTING               */
/**************************************/

void sort ( const TestInfo *ti )
{
	long size = GET_M(ti);
	int a[ size ];
	scatter( ti, a, &size );
}

void mainSort( const TestInfo *ti, int *a, long size )
{	
	scatter( ti, a, &size );
}
