
#include <limits.h>
#include "common.h"

#define DEBUG

/* Keep C++ compilers from getting confused */
#if defined(__cplusplus)
extern "C" {
#endif

//efficiently computes the logarithm base 2 of a positive integer
inline int _log2 ( unsigned int n )
{
    unsigned int toRet = 0;
    int m = n - 1;
    while (m > 0) {
		m >>= 1;
		toRet++;
    }
    return toRet;
}
inline int _pow2( int n ) {
	int i, r = 1;
	for( i = 0; i < n; ++ i ) {
		r *= 2;
	}
	return r;
}

//computes the logarithm base k of a positive integer
inline int _logk ( unsigned int n, unsigned int k )
{
    return _log2 ( n ) / _log2 ( k );
}


//compare two integers
inline int compare (const void * a, const void * b)
{
  return ( *(int*)a - *(int*)b );
}

//returns 1 if x is a power of two, 0 otherwise
inline int isPowerOfTwo( int x )
{
    return x > 0 && (x & (x - 1)) == 0;
}














/***************************************************************************************************************/
/******************************************* Sequential Sort ***************************************************/
/***************************************************************************************************************/

long readNextBlock( Data *data, Data *dstbuffer, long size, long displ )
{
	SPD_ASSERT( dstbuffer->medium == Array, "buffer should be allocated in memory" );
	SPD_ASSERT( dstbuffer->array.size >= size+displ, "buffer size exceeded" );

	return fread( dstbuffer->array.data+displ, sizeof(int), size, data->file.handle );
}

long writeNextBlock( Data *data, Data *srcBuffer, long size, long displ )
{
	SPD_ASSERT( srcBuffer->medium == Array, "buffer should be allocated in memory" );
	SPD_ASSERT( srcBuffer->array.size >= size+displ, "buffer size exceeded" );

	return fwrite( srcBuffer->array.data+displ, sizeof(int), size, data->file.handle );;
}


typedef struct {
	int val;
	long run_index;
} Min_val;
typedef struct {
	int size;
	Min_val *elements;

} Heap;
void HeapInit( Heap *h, int maxElements ) {
	h->elements = (Min_val*)malloc( (maxElements+1) * sizeof(Min_val) );
	h->size = 0;
	h->elements[0].val = -INT_MAX;
}
void HeapDestroy( Heap *h ) {
	free( h->elements );
}
void HeapPush( Heap *h, int val, int index ) {
	int i;
	for ( i=++h->size; h->elements[i/2].val > val; i/=2)
        h->elements[i] = h->elements[i/2];
    h->elements[i].val = val;
	h->elements[i].run_index = index;
}
Min_val HeapTop( Heap *h ) {
	return h->elements[1];
}
void HeapPop( Heap *h ) {
	if ( ! h->size )
		return;
	Min_val *min = h->elements+1;
	Min_val *last = h->elements+(h->size--);
	int i, child;

	for ( i=1; i*2 <= h->size; i=child ) {
		child = i * 2;

		if ( child < h->size && h->elements[child+1].val < h->elements[child].val )
			child++;
		if ( last->val > h->elements[child].val )
			h->elements[i] = h->elements[child];
		else
			break;
	}
	h->elements[i] = *last;
}


void fileKMerge( Data *run_devices, const int k, const long runSize, const long dataSize, Data *merged_device ) {
	const long bufferedRunSize = runSize / (k + 1); //Size of a single buffered run (+1 because of the output buffer)

	if ( k > runSize ) {
		/* TODO: Handle this case */
		SPD_ASSERT( 0, "fileKMerge function doesn't allow a number of runs greater than memory buffer size" );
	}

	/* Runs Buffer */
	Data runs_buffer, output_buffer;
	DAL_init( &runs_buffer );
	DAL_allocArray( &runs_buffer, bufferedRunSize * k );
	int* runs = runs_buffer.array.data;

	/* Output buffer */
	DAL_init( &output_buffer );
	DAL_allocArray( &output_buffer, bufferedRunSize );
	int* output = output_buffer.array.data;

	/* Indexes for the k buffered runs */
	long *runs_indexes = (long*) calloc( sizeof(long), k );

	/* The auxiliary heap struct */
	Heap heap;
	HeapInit( &heap, k );

	long i;

	/* Initializing the buffered runs */
	for ( i=0; i < k; i++ )
		readNextBlock( &run_devices[i], &runs_buffer, bufferedRunSize, i*bufferedRunSize );

	/* Initializing the heap */
    for ( i=0; i<k; i++ )
		HeapPush( &heap, runs[i*bufferedRunSize], i );

	/* Merging the runs */
	int outputSize = 0;
    for ( i=0; i<dataSize; i++ ) {
        Min_val min = HeapTop( &heap );
        HeapPop( &heap );

		//the run index
		int j = min.run_index;

        if ( ++(runs_indexes[j]) < bufferedRunSize )							//If there are others elements in the buffered run
			HeapPush( &heap, runs[j*bufferedRunSize+runs_indexes[j]], j );		//pushes a new element in the heap
		else if (readNextBlock( &run_devices[j], &runs_buffer, bufferedRunSize, j*bufferedRunSize ) ) {	//else, if the run has not been read completely
			runs_indexes[j] = 0;
			HeapPush( &heap, runs[j*bufferedRunSize], j );
		}

        output[outputSize++] = min.val;

		if ( (outputSize % bufferedRunSize) == 0 || i==dataSize-1 ) {										//If the output buffer is full
			SPD_ASSERT( writeNextBlock( merged_device,
										&output_buffer,
										outputSize,
										0 )
										== outputSize, "fwrite fails" );	//writes it into the merged Data device

			outputSize = 0;
		}
    }
	/* Freeing memory */
	HeapDestroy( &heap );
    DAL_destroy( &runs_buffer );
	DAL_destroy( &output_buffer );
	free( runs_indexes );
}

void initRuns( Data *run_devices, int k, int size ) {
	int i;
	for ( i=0; i<k; i++ ) {
		DAL_init( &run_devices[i] );
		SPD_ASSERT( DAL_allocFile( &run_devices[i], size ), "couldn't create a temporary file for runs" );
	}
}

void destroyRuns( Data *run_devices, int k ) {
	int i;
	for ( i=0; i<k; i++ )
		DAL_destroy( &run_devices[i] );
}

void fileSort( Data *data )
{
	const long dataSize = DAL_dataSize( data );

	/* Memory buffer */
	Data buffer;
	DAL_init( &buffer );
 	SPD_ASSERT( DAL_allocBuffer( &buffer, dataSize ), "not enough memory..." );

	const long runSize = DAL_dataSize( &buffer );	//Single run size
	const int k = dataSize / runSize;				//Number of runs_

	/* Data that will contain temporary runs */
	Data run_devices[k];
	initRuns( run_devices, k, runSize );

	/* Stub for reading/writing the sorted data */
	Data dataStub;
	dataStub.medium = File;
	dataStub.file.handle = fopen( data->file.name, "r+b" );
	if( ! dataStub.file.handle ) {
		SPD_DEBUG( "Cannot open \"%s\" for reading.", data->file.name );
		return;
	}

	long readSize = 0;
	int i;
	/* Sorting single runs */
	for( i=0; i<k; i++ ) {
		readSize = readNextBlock( &dataStub, &buffer, runSize, 0 );
		qsort( buffer.array.data, readSize, sizeof(int), compare );
		writeNextBlock( &run_devices[i], &buffer, readSize, 0 );
		DAL_resetDeviceCursor( &run_devices[i] );
	}
	DAL_destroy( &buffer );
	DAL_resetDeviceCursor( &dataStub );

 	fileKMerge( run_devices, k, runSize, dataSize, &dataStub );		//k-way merge

	fclose( dataStub.file.handle );
	destroyRuns( run_devices, k );

// 	//DEBUG
// 	DAL_PRINT_DATA( data, "sorted data" );
}

/// sequential sort function
void sequentialSort( const TestInfo *ti, Data *data )
{
	switch( data->medium ) {
		case File: {
			fileSort( data );
			break;
		}
		case Array: {
			qsort( data->array.data, data->array.size, sizeof(int), compare );
			break;
		}
		default:
			DAL_UNSUPPORTED( data );
	}
}
/***************************************************************************************************************/














/**
* @brief Gets the index of the bucket in which to insert ele
*
* @param[in] ele        The element to be inserted within a bucket
* @param[in] splitters  The array of splitters
* @param[in] length     The number of splitters
*
* @return The index of the bucket in which to insert ele
*/
int getBucketIndex( const int *ele, const int *splitters, const long length )
{
	int low = 0;
	int high = length;
	int cmpResult = 0;
	int mid;

	while ( high > low ) {
		mid = (low + high) >> 1;
		cmpResult = compare( ele, &splitters[mid] );

		if ( cmpResult == 0 )
			return mid;

		if ( cmpResult > 0 )
			low = mid + 1;
		else
			high = mid - 1;
	}
	cmpResult = compare( ele, &splitters[low] );

	if ( cmpResult > 0 && low < length)
		return (low + 1);
	return low;
}

/**
* @brief Chooses n-1 equidistant elements of the input data array as splitters
*
* @param[in] 	array 				Data from which extract splitters
* @param[in] 	length 				Length of the data array
* @param[in] 	n 					Number of parts in which to split data
* @param[out] 	newSplitters	 	Chosen splitters
*/
void chooseSplitters( int *array, const long length, const int n, int *newSplitters )
{
	int i, j, k;

	if ( length >= n )
		/* Choosing splitters (n-1 equidistant elements of the data array) */
		for ( i=0, k=j=length/n; i<n-1; i++, k+=j )
			newSplitters[i] = array[k];
	else {
		/* Choosing n-1 random splitters */
		for ( i=0; i<n-1; i++ )
			newSplitters[i] = array[rand()%length];
		qsort( newSplitters, n-1, sizeof(int), compare );
	}
}

/**
* @brief Chooses n-1 equidistant elements of the input data object as splitters
*
* @param[in] 	data 				Data from which extract splitters
* @param[in] 	n 					Number of parts in which to split data
* @param[out] 	newSplitters	 	Chosen splitters
*/
void chooseSplittersFromData( Data *data, const int n, int *newSplitters )
{
	/* TODO: Implement it the right way!! */

	switch( data->medium ) {
		case File: {
			DAL_UNIMPLEMENTED( data );
			break;
		}
		case Array: {
			chooseSplitters( data->array.data, data->array.size, n, newSplitters );
			break;
		}
		default:
			DAL_UNSUPPORTED( data );
	}
}

/* Keep C++ compilers from getting confused */
#if defined(__cplusplus)
}
#endif