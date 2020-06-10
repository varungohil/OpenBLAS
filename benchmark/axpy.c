/***************************************************************************
Copyright (c) 2014, The OpenBLAS Project
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
3. Neither the name of the OpenBLAS project nor the names of
its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE OPENBLAS PROJECT OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#ifdef __CYGWIN32__
#include <sys/time.h>
#endif
#include "common.h"


#undef AXPY

#ifdef COMPLEX
#ifdef DOUBLE
#define AXPY   BLASFUNC(zaxpy)
#define FILEX   "zaxpy_x.txt"
#define FILEY   "zaxpy_y.txt"
#define FILER   "zaxpy_res.txt"
#define FORMAT  "%lf\n"
#else
#define AXPY   BLASFUNC(caxpy)
#define FILEX   "caxpy_x.txt"
#define FILEY   "caxpy_y.txt"
#define FILER   "caxpy_res.txt"
#define FORMAT  "%.14f\n"
#endif
#else
#ifdef DOUBLE
#define AXPY   BLASFUNC(daxpy)
#define FILEX   "daxpy_x.txt"
#define FILEY   "daxpy_y.txt"
#define FILER   "daxpy_res.txt"
#define FORMAT  "%lf\n"
#else
#define AXPY   BLASFUNC(saxpy)
#define FILEX   "saxpy_x.txt"
#define FILEY   "saxpy_y.txt"
#define FILER   "saxpy_res.txt"
#define FORMAT  "%.14f\n"
#endif
#endif

#if defined(__WIN32__) || defined(__WIN64__)

#ifndef DELTA_EPOCH_IN_MICROSECS
#define DELTA_EPOCH_IN_MICROSECS 11644473600000000ULL
#endif

int gettimeofday(struct timeval *tv, void *tz){

  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag;

  if (NULL != tv)
    {
      GetSystemTimeAsFileTime(&ft);

      tmpres |= ft.dwHighDateTime;
      tmpres <<= 32;
      tmpres |= ft.dwLowDateTime;

      /*converting file time to unix epoch*/
      tmpres /= 10;  /*convert into microseconds*/
      tmpres -= DELTA_EPOCH_IN_MICROSECS;
      tv->tv_sec = (long)(tmpres / 1000000UL);
      tv->tv_usec = (long)(tmpres % 1000000UL);
    }

  return 0;
}

#endif

#if !defined(__WIN32__) && !defined(__WIN64__) && !defined(__CYGWIN32__) && 0

static void *huge_malloc(BLASLONG size){
  int shmid;
  void *address;

#ifndef SHM_HUGETLB
#define SHM_HUGETLB 04000
#endif

  if ((shmid =shmget(IPC_PRIVATE,
		     (size + HUGE_PAGESIZE) & ~(HUGE_PAGESIZE - 1),
		     SHM_HUGETLB | IPC_CREAT |0600)) < 0) {
    printf( "Memory allocation failed(shmget).\n");
    exit(1);
  }

  address = shmat(shmid, NULL, SHM_RND);

  if ((BLASLONG)address == -1){
    printf( "Memory allocation failed(shmat).\n");
    exit(1);
  }

  shmctl(shmid, IPC_RMID, 0);

  return address;
}

#define malloc huge_malloc

#endif

int main(int argc, char *argv[]){

  FLOAT *x, *y;
  FLOAT alpha[2] = { 2.0, 2.0 };
  blasint m, i;
  blasint inc_x=1,inc_y=1;
  int loops = 1;
  int l;
  char *p;

  int from =   1;
  int to   = 200;
  int step =   1;
  int random_input = 0;
	
  struct timespec start, stop;
  double time1,timeg;

  argc--;argv++;
  if (argc > 0) { random_input = atol(*argv);    argc--; argv++; }
  if (argc > 0) { from     = atol(*argv);		argc--; argv++;}
  if (argc > 0) { to       = MAX(atol(*argv), from);	argc--; argv++;}
  if (argc > 0) { step     = atol(*argv);		argc--; argv++;}

  if ((p = getenv("OPENBLAS_LOOPS")))  loops = atoi(p);
  if ((p = getenv("OPENBLAS_INCX")))   inc_x = atoi(p);
  if ((p = getenv("OPENBLAS_INCY")))   inc_y = atoi(p);

  fprintf(stderr, "From : %3d  To : %3d Step = %3d Inc_x = %d Inc_y = %d Loops = %d\n", from, to, step,inc_x,inc_y,loops);

  if (( x = (FLOAT *)malloc(sizeof(FLOAT) * to * abs(inc_x) * COMPSIZE)) == NULL){
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }

  if (( y = (FLOAT *)malloc(sizeof(FLOAT) * to * abs(inc_y) * COMPSIZE)) == NULL){
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }

#ifdef linux
  srandom(getpid());
#endif

  fprintf(stderr, "   SIZE       Flops\n");

  for(m = from; m <= to; m += step)
  {

   timeg=0;

   fprintf(stderr, " %6d : ", (int)m);

   FILE *fp;
   for (l=0; l<loops; l++)
   {
        if(random_input)
	{
		fp = fopen(FILEX,"w");
		for(i = 0; i < m * COMPSIZE * abs(inc_x); i++){
				x[i] = ((FLOAT) rand() / (FLOAT) RAND_MAX) - 0.5;
			fprintf(fp, FORMAT, x[i]);
		}
		fclose(fp);

		fp = fopen(FILEY,"w");
		for(i = 0; i < m * COMPSIZE * abs(inc_y); i++){
				y[i] = ((FLOAT) rand() / (FLOAT) RAND_MAX) - 0.5;
			fprintf(fp, FORMAT, y[i]);
		}
		fclose(fp);
		clock_gettime( CLOCK_REALTIME, &start);

		AXPY (&m, alpha, x, &inc_x, y, &inc_y );

		clock_gettime( CLOCK_REALTIME, &stop);
		fp = fopen(FILER,"w");
		for(i = 0; i < m * COMPSIZE * abs(inc_y); i++){
			fprintf(fp, FORMAT, y[i]);
		}
		fclose(fp);

		time1 = (double)(stop.tv_sec - start.tv_sec) + (double)((stop.tv_nsec - start.tv_nsec)) * 1.e-9;

		timeg += time1;
	}
	else
	{
		fp = fopen(FILEX,"r");
		for(i = 0; i < m * COMPSIZE * abs(inc_x); i++){
			fscanf(fp, "%f\n", &x[i]);
		}
		fclose(fp);

		fp = fopen(FILEY,"r");
		for(i = 0; i < m * COMPSIZE * abs(inc_y); i++){
			fscanf(fp, "%f\n", &y[i]);
		}
		fclose(fp);
		clock_gettime( CLOCK_REALTIME, &start);

		AXPY (&m, alpha, x, &inc_x, y, &inc_y );

		clock_gettime( CLOCK_REALTIME, &stop);
		fp = fopen(FILER,"w");
		for(i = 0; i < m * COMPSIZE * abs(inc_y); i++){
			fprintf(fp, FORMAT, y[i]);
		}
		fclose(fp);

		time1 = (double)(stop.tv_sec - start.tv_sec) + (double)((stop.tv_nsec - start.tv_nsec)) * 1.e-9;

		timeg += time1;
	}
    }

    timeg /= loops;

    fprintf(stderr,
	    " %10.2f MFlops %10.9f sec\n",
	    COMPSIZE * COMPSIZE * 2. * (double)m / timeg * 1.e-6, timeg);

  }

  return 0;
}

// void main(int argc, char *argv[]) __attribute__((weak, alias("MAIN__")));
