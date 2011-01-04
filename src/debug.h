

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include<assert.h>

/* Keep C++ compilers from getting confused */
#if defined(__cplusplus)
extern "C" {
#endif


void print_trace( void ); // TODO: pure debugging, move in debug.c ...

#define SPD_COLOR_DEFAULT	"\E[0m"
#define SPD_COLOR_RED		"\E[1m\E[31m"
#define SPD_COLOR_GREEN		"\E[1m\E[32m"
#define SPD_COLOR_YELLOW	"\E[1m\E[33m"

#define SPD_ASSERT(cond, fmt, ... ) \
	if( ! (cond) ) { \
		SPD_ERROR( fmt "\n  (" ##cond "failed)", ##__VA_ARGS__ ); \
	}

#define SPD_BASE_ERROR_STR SPD_COLOR_YELLOW "%s:%d" SPD_COLOR_DEFAULT ": " SPD_COLOR_RED "%s()" SPD_COLOR_DEFAULT ": "
#define SPD_BASE_ERROR_ARGS __FILE__, __LINE__, __FUNCTION__

#define SPD_ERROR( fmt, ... ) \
	{ \
		char buf[1024]; \
		printf( SPD_BASE_ERROR_STR fmt "\n", SPD_BASE_ERROR_ARGS, ##__VA_ARGS__ ); \
		print_trace(); \
		exit(1); \
	}

/* Keep C++ compilers from getting confused */
#if defined(__cplusplus)
}
#endif

#endif

