
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <sys/time.h>
#include "sorting.h"

#define _GNU_SOURCE

static const struct option long_options[] =
{
	{"version", no_argument,       0, 'V'},
	{"help",    no_argument,       0, 'h'},

	{"verbose", no_argument,       0, 'v'},
	{"threaded",no_argument,       0, 't'},
	{"mpionly", no_argument,       0, 'o'},
	// {"nodes",   required_argument, 0, 'n'}, // handled by MPI
	{"size",    required_argument, 0, 'M'},
	{"seed",    required_argument, 0, 's'},
	{"algo",    required_argument, 0, 'a'},
	{0, 0, 0, 0}
};
void printHelp( const char *argv0 )
{
	printf( "Usage:\n"
			"\t%s [options]\n"
			"\n"
			"Options:\n"
			"\t--version  -V        print program version and exit\n"
			"\t--help     -h        print this help and exit\n"
			"\t--verbose  -v        print more informations\n"
			"\t--threaded -t        use a shared-memory verison of algorithm between CPU cores\n"
			"\t--mpionly  -o        use only MPI algorithm between both cores and CPUs\n"
			// "\t--nodes    -n  N  use n nodes\n" // handled by MPI -- see below
			"\t--size     -M  M     sort M elements\n"
			"\t--seed     -s  S     use seed S to generate random elements\n"
			"\t--algo     -a  A     use algorithm A\n"
			"\t--av1      -1  X     pass to the algorithm X as first variable\n"
			"\t--av2      -2  Y     pass to the algorithm Y as second variable\n"
			"\t--av3      -3  Z     pass to the algorithm Z as third variable\n"
			"\n", argv0 );
}

void printVersion( )
{
	srand( time(NULL) );

	#define COUNT 3
	char * (authors[COUNT]) = { "Paolo Giangrandi", "Fabio Luporini", "Nicola Desogus" };
	int i;

	for( i = 0; i < COUNT; ++ i ) {
		int r = rand() % ( COUNT - i ) + i;

		#define SWAP( a, b ) { char*tmp=(a); (a)=(b); (b)=tmp; }
		SWAP( authors[ i ], authors[ r ] );
	}

	printf( "SPD Project version 0.0.01\n"
			"Written by:\n" );
	for( i = 0; i < COUNT; ++ i ) {
		printf( "\t%s\n", authors[i] );
	}
	printf( "Released under the terms of GPL version 3 or higher\n" );
}

long strToInt( const char *str, bool *err )
{
	char *end;
	long r = strtol( str, &end, 10 );

	*err = ! ( *str != '\0' && *end == '\0' );

	return r;
}

// return -1 on error, 0 and 1 on success
// if 0, program must quit anyway
int parseArgs( int argc, char **argv, TestInfo *ti )
{
	ti->verbose = 0;
	ti->threaded = 0;
	memset( ti->algoVar, 0, sizeof(ti->algoVar) );

	bool MGiven = 0, seedGiven = 0, algoGiven = 0;

	while( 1 ) {
		/* getopt_long stores the option index here. */
		int option_index = 0;
		int c;

		bool err = 0;

		// c = getopt_long( argc, argv, "Vhvton:M:s:a:", long_options, &option_index );
		c = getopt_long( argc, argv, "VhvtoM:s:a:1:2:3:", long_options, &option_index );

		/* Detect the end of the options. */
		if( c == -1 ) {
			break;
		}

		switch( c ) {
			case 'V':
				printVersion( );
				return 0;

			case 'h':
				printHelp( argv[0] );
				return 0;

			case 'v':
				ti->verbose = 1;
				break;

			case 't':
				ti->threaded = 1;
				break;

			case 'o':
				ti->threaded = 0;
				break;

			/*
			case 'n':
				printf ("option -n with value `%s'\n", optarg);
				break;
			*/

			case 'M':
				ti->M = strToInt( optarg, &err );
				MGiven = 1;
				if( err ) {
					printf( "M is not a valid number.\n" );
					return -1;
				}
				break;

			case 's':
				ti->seed = strToInt( optarg, &err );
				seedGiven = 1;
				if( err ) {
					printf( "seed is not a valid number.\n" );
					return -1;
				}
				break;

			case 'a':
				strncpy( ti->algo, optarg, sizeof(ti->algo) );
				algoGiven = 1;
				break;
				
			case '1':
				ti->algoVar[0] = strToInt( optarg, &err );
				break;
			case '2':
				ti->algoVar[1] = strToInt( optarg, &err );
				break;
			case '3':
				ti->algoVar[2] = strToInt( optarg, &err );
				break;

			case '?':
				/* getopt_long already printed an error message. */
				return -1;

			default:
				printf( "WTF!?\n" );
		}
	}

	#define ERROR { printf( "\n" ); printHelp( argv[0] ); return 1; }
	if( ! MGiven ) {
		printf( "M is mandatory!\n" );
		return -1;
	}
	else if( GET_M(ti) < GET_N(ti) ) {
		printf( "M must be greater or equal to the number of nodes\n" );
		return -1;
	}
	if( ! seedGiven ) {
		printf( "seed is mandatory!\n" );
		return -1;
	}
	if( ! algoGiven ) {
		printf( "algo is mandatory!\n" );
		return -1;
	}

	return 1;
}

bool checkAlgo( const char *algo )
{
	char path[1024];
	SortFunction sort;
	void *handle = dlopen( GET_ALGORITHM_PATH(algo, path, sizeof(path)), RTLD_LAZY | RTLD_GLOBAL );

	if( ! handle ) {
		printf( "Couldn't load %s (%s): %s\n"
				"%s\n", algo, path, strerror(errno), dlerror() );
		return 0;
	}

	sort = (SortFunction) dlsym( handle, "sort" );
	if( ! sort ) {
		printf( "Couldn't load %s (%s)'s sort() function: %s\n"
				"%s\n", algo, path, strerror(errno), dlerror() );
		dlclose( handle );
		return 0;
	}

	dlclose( handle );
	return 1;
}

static unsigned int _seed_x = 123456789, _seed_y = 987654321, _seed_z = 43219876, _seed_c = 6543217; /* Seed variables */
int JKISS()
{
	unsigned long long t;
	_seed_x = 314527869 * _seed_x + 1234567;
	_seed_y ^= _seed_y << 5; _seed_y ^= _seed_y >> 7; _seed_y ^= _seed_y << 22;
	t = 4294584393ULL * _seed_z + _seed_c; _seed_c = t >> 32; _seed_z = t;
	return (_seed_x + _seed_y + _seed_z)  % INT_MAX;
}

int generate( const TestInfo *ti )
{
	// TODO!
	// implement it the right way :F

	FILE *f;
	char path[1024];
	long M = GET_M(ti), i;

	// TODO: check if file is already existing and if it's valid

	GET_UNSORTED_DATA_PATH( ti, path, sizeof(path) );
	f = fopen( path, "wb" );
	if( ! f ) {
		printf( "Couldn't open %s for writing\n", path );
		return 0;
	}

	/* Setting seed variables */
	_seed_x = ti->seed;
	_seed_y = 69069*_seed_x+12345;
	_seed_z = (_seed_y<<16)+_seed_x;
	_seed_c = _seed_z<<13;

	for( i = 0; i < M; ++ i ) {
		int x = JKISS();
#ifndef DEBUG
		if( ! fwrite( &x, sizeof(int), 1, f ) ) {
#else
		if( fprintf( f, "%d\n", x ) < 0 ) {
#endif
			printf( "Couldn't write %ld-th element (of value %d) to %s\n", i, x, path );
			return 0;
		}
	}

	fclose( f );

	return 1;
}
int loadData( const TestInfo *ti, int **data, long *size )
{
	FILE *f;
	char path[1024];
#ifndef DEBUG
#else
	long i;
#endif

	GET_UNSORTED_DATA_PATH( ti, path, sizeof(path) );
	f = fopen( path, "rb" );
	if( ! f ) {
		printf( "Couldn't open %s for reading\n", path );
		return 0;
	}

#ifndef DEBUG
	*size = GET_FILE_SIZE( path );
	if( (unsigned long) *size != GET_M(ti)*sizeof(int) ) {
		printf( "%s should be of %ld bytes (%ld elements), while it is %ld bytes\n",
				path, GET_M(ti)*sizeof(int), GET_M(ti), *size );
		fclose( f );
		return 0;
	}
	*data = (int*) malloc( *size );

	if( ! fread( *data, *size, 1, f ) ) {
		printf( "Couldn't read %ld bytes from %s\n", *size, path );
		fclose( f );
		return 0;
	}
#else
	*size = GET_M(ti)*sizeof(int);
	*data = (int*) malloc( *size );
	for( i = 0; i < GET_M(ti); ++ i ) {
		if( fscanf( f, "%d ", & (*data)[i] ) == EOF ) {
			printf( "Couldn't read %ld-th element (of value %d) from %s\n", i, (*data)[i], path );
			fclose( f );
			return 0;
		}
	}
#endif

	fclose( f );

	return 1;
}
int storeData( const TestInfo *ti, int *data, long size )
{
	FILE *f;
	char path[1024];
#ifndef DEBUG
#else
	(void) size;
	long i;
	int tmp;
#endif

	GET_SORTED_DATA_PATH( ti, path, sizeof(path) );
	f = fopen( path, "wb" );
	if( ! f ) {
		printf( "Couldn't open %s for writing\n", path );
		return 0;
	}

#ifndef DEBUG
	if( ! fwrite( data, size, 1, f ) ) {
		printf( "Couldn't write %ld bytes to %s\n", size, path );
		fclose( f );
		return 0;
	}
#else
	if( fprintf( f, "%d\n", data[0] ) < 0 ) {
		printf( "Couldn't write %d-th element (of value %d) to %s\n", 0, data[0], path );
		fclose( f );
		return 0;
	}
	tmp = data[0];

	for( i = 1; i < GET_M(ti); ++ i ) {
		if( tmp > data[i] ) {
			printf( "Sorting Failed: %ld-th element (of value %d) is bigger than %ld-th element (of value %d)\n", i-1, tmp, i, data[i] );
			fclose( f );
			return 0;
		}
		tmp = data[i];

		if( fprintf( f, "%d\n", data[i] ) < 0 ) {
			printf( "Couldn't write %ld-th element (of value %d) to %s\n", i, data[i], path );
			fclose( f );
			return 0;
		}
	}
#endif

	fclose( f );

	return 1;
}


/*
 *TIMING STUFF
 */

typedef struct _Phase
{
	struct timeval start, end;
} Phase;

static Phase *phases = 0;
static char **phaseNames = 0;
static int phaseCount = 0;
#ifdef DEBUG
static int phaseEndCount = 0;
#endif

PhaseHandle startPhase( const TestInfo *ti, const char *phaseName )
{
	int phaseId = phaseCount;
	phaseCount ++;
	
	if( GET_ID(ti) == 0 ) {
		phaseNames = realloc( phaseNames, sizeof(char*)*phaseCount );
		phaseNames[ phaseId ] = strdup( phaseName );
	}
	phases = realloc( phases, sizeof(Phase)*phaseCount );
	Phase *p = & phases[ phaseId ];
	
	gettimeofday( & p->start, NULL );
	
	return phaseId;
}
void stopPhase( const TestInfo *ti, PhaseHandle phase )
{
	gettimeofday( & phases[ phase ].end, NULL );
#ifdef DEBUG
	phaseEndCount ++;
#endif
}



int main( int argc, char **argv )
{
	/* TODO:
	 * use MPI_Pack and MPI_Unpack to send ti... */
	TestInfo ti;
	int r;

	MPI_Init( &argc, &argv );

	if( GET_ID(&ti) == 0 ) {
		// parse arguments
		r = parseArgs( argc, argv, &ti );
		if( r == -1 ) {
			printf( "\n" );
			printHelp( argv[0] );
			MPI_Finalize( );
			return 1;
		}
		else if( r == 0 ) {
			MPI_Finalize( );
			return 0;
		}

		if( ! checkAlgo( ti.algo ) ) {
			printf( "\n" );
			printHelp( argv[0] );
			MPI_Finalize( );
			return 1;
		}

		// broadcasting ti
		MPI_Bcast( &ti, sizeof(TestInfo), MPI_CHAR, 0, MPI_COMM_WORLD );

		r = generate( &ti );
		if( ! r ) {
			printf( "Error generating data for task %d: %s\n", GET_ID(&ti), strerror(errno) );
			MPI_Finalize( );
			return 1;
		}

		// sorting data
		{
			int *data;
			long size;

			r = loadData( &ti, &data, &size );
			if( ! r ) {
				printf( "Error loading data for task %d\n", GET_ID(&ti) );
				MPI_Finalize( );
				return 1;
			}

			{
				char path[1024];
				MainSortFunction mainSort;
				PhaseHandle mainSortPhase;

				void *handle = dlopen( GET_ALGORITHM_PATH(ti.algo, path, sizeof(path)), RTLD_LAZY | RTLD_GLOBAL );
				if( ! handle ) {
					printf( "Task %d couldn't load %s (%s): %s\n"
							"%s\n", GET_ID(&ti), ti.algo, path, strerror(errno), dlerror() );
					free ( data );
					MPI_Finalize( );
					return 1;
				}

				// sorting!
				mainSort = (MainSortFunction) dlsym( handle, "mainSort" );
				if( ! mainSort ) {
					printf( "Task %d couldn't load %s (%s)'s mainSort() function: %s\n"
							"%s\n", GET_ID(&ti), ti.algo, path, strerror(errno), dlerror() );
					dlclose( handle );
					free ( data );
					MPI_Finalize( );
					return 1;
				}
				
				mainSortPhase = startPhase( &ti, "sort" );
				mainSort( &ti, data, size / sizeof(int) );
				stopPhase( &ti, mainSortPhase );

				dlclose( handle );
			}

			r = storeData( &ti, data, size );
			if( ! r ) {
				printf( "Error storing data for task %d\n", GET_ID(&ti) );
				free ( data );
				MPI_Finalize( );
				return 1;
			}
			free ( data );
		}

		// gathering phases
		{
			Phase **allPhases = (Phase**) malloc( sizeof(Phase*)*GET_N(&ti) );
			int i, j;
			
			allPhases[0] = (Phase*) malloc( sizeof(Phase)*phaseCount );
			memcpy( allPhases[0], phases, sizeof(Phase)*phaseCount );
			
#ifdef DEBUG
			if( phaseCount != phaseEndCount ) {
				printf( "Warning! Node %d started %d phases, and stopped %d only.",
						GET_ID(&ti), phaseCount, phaseEndCount );
			}
#endif
			
			for( i = 1; i < GET_N(&ti); ++ i ) {
#ifdef DEBUG
				int count;
				// getting how many phases they've got ...
				MPI_Recv( & count, sizeof(int), MPI_CHAR, i, 0, MPI_COMM_WORLD, NULL );
				if( count != phaseCount ) {
					printf( "Warning! Node %d has only %d phases, while node 0 has %d.",
							i, count, phaseCount );
				}
#endif
				allPhases[i] = (Phase*) malloc( sizeof(Phase)*phaseCount );
				MPI_Recv( allPhases[i], sizeof(Phase)*phaseCount, MPI_CHAR, i, 0, MPI_COMM_WORLD, NULL );
			}
			
			// printing phases
			for( i = 0; i < phaseCount; ++ i ) {
				printf( "Phase \"%s\":\n", phaseNames[i] );
				for( j = 0; j < GET_N(&ti); ++ j ) {
					Phase *p = & ( allPhases[j][i] );
					struct timeval t1 = p->start, t2 = p->end;
					long utime, mtime, time, secs, usecs;
	
					secs  = t2.tv_sec  - t1.tv_sec;
					usecs = t2.tv_usec - t1.tv_usec;
	
					utime = secs*1000000 + usecs;
					mtime = (secs*1000 + usecs/1000.0) + 0.5;
					time = secs + usecs/1000000.0 + 0.5;
	
					printf( "   node %2d: %7ld microsecs :: %4ld millisecs :: %ld secs\n", j, utime, mtime, time );
				}
			}
			
			// freeing phase data
			for( i = 0; i < GET_N(&ti); ++ i ) {
				free( allPhases[i] );
			}
			free( allPhases );
		}
		
		// freeing global data
		{
			int i;
			
			for( i= 0; i < phaseCount; ++ i ) {
				free( phaseNames[i] );
			}
			free( phaseNames );
			free( phases );
		}
	}
	else {
		// receive ti
		MPI_Bcast( &ti, sizeof(TestInfo), MPI_CHAR, 0, MPI_COMM_WORLD );

		// sorting
		{
			char path[1024];
			SortFunction sort;
			PhaseHandle sortPhase;

			void *handle = dlopen( GET_ALGORITHM_PATH(ti.algo, path, sizeof(path)), RTLD_LAZY | RTLD_GLOBAL );
			if( ! handle ) {
				printf( "Task %d couldn't load %s (%s): %s\n"
						"%s\n", GET_ID(&ti), ti.algo, path, strerror(errno), dlerror() );
				MPI_Finalize( );
				return 1;
			}

			// sorting!
			sort = (SortFunction) dlsym( handle, "sort" );
			if( ! sort ) {
				printf( "Task %d couldn't load %s (%s)'s sort() function: %s\n"
						"%s\n", GET_ID(&ti), ti.algo, path, strerror(errno), dlerror() );
				dlclose( handle );
				MPI_Finalize( );
				return 1;
			}

			sortPhase = startPhase( &ti, "sort" );
			sort( &ti );
			stopPhase( &ti, sortPhase );

			dlclose( handle );
		}
		
		// sending phases
		{
#ifdef DEBUG
			if( phaseCount != phaseEndCount ) {
				printf( "Warning! Node %d started %d phases, and stopped %d only.",
						GET_ID(&ti), phaseCount, phaseEndCount );
			}
			// sending how many phases I've got ...
			MPI_Send( & phaseCount, sizeof(int), MPI_CHAR, 0, 0, MPI_COMM_WORLD );
#endif
			MPI_Send( phases, sizeof(Phase)*phaseCount, MPI_CHAR, 0, 0, MPI_COMM_WORLD );
		}
		
		// freeing global data
		{
			free( phases );
		}

	}

	MPI_Finalize( );
	return 0;
}