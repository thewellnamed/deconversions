/*
 * Copyright (c) AT&T Corp. 1994, 1995.
 * This code is licensed by AT&T Corp.  For the
 * terms and conditions of the license, see
 * http://www.research.att.com/orgs/ssr/book/reuse
 */

#ifndef DOS

#include	<sys/types.h>
#include	<sys/times.h>
#include	<sys/param.h>
#ifndef HZ
#define HZ 60
#endif
typedef struct tms mytime_t;
#define GET_TIME(S) times(&(S))
#define DIFF_IN_SECS(S,T) ((S.tms_utime + S.tms_stime - T.tms_utime - T.tms_stime)/(double)HZ)

#else

#include	<time.h>
typedef clock_t mytime_t;
#define GET_TIME(S) S = clock()
#define DIFF_IN_SECS(S,T) ((S - T) / (double)CLOCKS_PER_SEC)

#endif
	
static mytime_t T;

start_timer()
{
	GET_TIME(T);
}

double
elapsed_sec()
{
	mytime_t	S;
	double		rv;

	GET_TIME(S);
	rv = DIFF_IN_SECS(S,T);
	return rv;
}
