
#include <mpi.h>
#include "../debug.h"

#define TESTS_ERROR( ret, fmt, ... ) \
	{ \
		SPD_DEBUG( fmt, ##__VA_ARGS__ ); \
		return ret; \
	}

// tiny wrappers for MPI
// these don't use Datas, memory arrays like MPI do, but are so much more comfortable for our purpose...
// taken from dal.c
static inline void TESTS_MPI_SEND( void *array, long size, MPI_Datatype dataType, int dest ) {
	MPI_Send( array, size, dataType, dest, 0, MPI_COMM_WORLD );
}
static inline void TESTS_MPI_RECEIVE( void *array, long size, MPI_Datatype dataType, int source ) {
	MPI_Status 	stat;
	MPI_Recv( array, size, dataType, source, 0, MPI_COMM_WORLD, &stat );
}

static inline int GET_ID ( ) {
    int x;
    MPI_Comm_rank ( MPI_COMM_WORLD, &x );
    return x;
}
static inline int GET_N ( ) {
    int x;
    MPI_Comm_size ( MPI_COMM_WORLD, &x );
    return x;
}

