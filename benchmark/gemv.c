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


#undef GEMV

#ifndef COMPLEX

#ifdef DOUBLE
#define GEMV   BLASFUNC(dgemv)
#define FILEA   "dgemv_a.txt"
#define FILEX   "dgemv_x.txt"
#define FILEY   "dgemv_y.txt"
#define FILER   "dgemv_res.txt"
#define FORMAT  "%lf\n"
#else
#define GEMV   BLASFUNC(sgemv)
#define FILEA   "sgemv_a.txt"
#define FILEX   "sgemv_x.txt"
#define FILEY   "sgemv_y.txt"
#define FILER   "sgemv_res.txt"
#define FORMAT  "%.14f\n"
#endif

#else

#ifdef DOUBLE
#define GEMV   BLASFUNC(zgemv)
#define FILEA   "zgemv_a.txt"
#define FILEX   "zgemv_x.txt"
#define FILEY   "zgemv_y.txt"
#define FILER   "zgemv_res.txt"
#define FORMAT  "%lf\n"
#else
#define GEMV   BLASFUNC(cgemv)
#define FILEA   "cgemv_a.txt"
#define FILEX   "cgemv_x.txt"
#define FILEY   "cgemv_y.txt"
#define FILER   "cgemv_res.txt"
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

  FLOAT *a, *x, *y;
  FLOAT alpha[] = {1.0, 1.0};
  FLOAT beta [] = {1.0, 0.0};
  char trans='N';
  blasint m, i, j;
  blasint inc_x=1,inc_y=1;
  blasint n=0;
  int has_param_n = 0;
  int has_param_m = 0;
  int loops = 1;
  int l;
  char *p;

  int from =   1;
  int to   = 200;
  int step =   1;
  int random_input = 0; //Varun added

  struct timeval start, stop;
  double time1,timeg;

  argc--;argv++;
  if (argc > 0) { random_input = atol(*argv);    argc--; argv++; }
  if (argc > 0) { from     = atol(*argv);		argc--; argv++;}
  if (argc > 0) { to       = MAX(atol(*argv), from);	argc--; argv++;}
  if (argc > 0) { step     = atol(*argv);		argc--; argv++;}


  int tomax = to;

  if ((p = getenv("OPENBLAS_LOOPS")))  loops = atoi(p);
  if ((p = getenv("OPENBLAS_INCX")))   inc_x = atoi(p);
  if ((p = getenv("OPENBLAS_INCY")))   inc_y = atoi(p);
  if ((p = getenv("OPENBLAS_TRANS")))  trans=*p;
  if ((p = getenv("OPENBLAS_PARAM_N"))) {
	  n = atoi(p);
	  if ((n>0)) has_param_n = 1;
  	  if ( n > tomax ) tomax = n;
  }
  if ( has_param_n == 0 )
  	if ((p = getenv("OPENBLAS_PARAM_M"))) {
		  m = atoi(p);
		  if ((m>0)) has_param_m = 1;
  	  	  if ( m > tomax ) tomax = m;
  	}



  fprintf(stderr, "From : %3d  To : %3d Step = %3d Trans = '%c' Inc_x = %d Inc_y = %d Loops = %d\n", from, to, step,trans,inc_x,inc_y,loops);

  if (( a = (FLOAT *)malloc(sizeof(FLOAT) * tomax * tomax * COMPSIZE)) == NULL){
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }

  if (( x = (FLOAT *)malloc(sizeof(FLOAT) * tomax * abs(inc_x) * COMPSIZE)) == NULL){
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }

  if (( y = (FLOAT *)malloc(sizeof(FLOAT) * tomax * abs(inc_y) * COMPSIZE)) == NULL){
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }

#ifdef linux
  srandom(getpid());
#endif

  fprintf(stderr, "   SIZE       Flops\n");
  FILE *fp; //Varun added
  FILE *fp2;

  if (has_param_m == 0)
  {

  	for(m = from; m <= to; m += step)
  	{
   		if(random_input){
			timeg=0;
			if ( has_param_n == 0 ) n = m;
			fprintf(stderr, " %6dx%d : ", (int)m,(int)n);
			fp = fopen(FILEA,"w");
			for(j = 0; j < m; j++){
				for(i = 0; i < n * COMPSIZE; i++){
					a[(long)j + (long)i * (long)m * COMPSIZE] = ((FLOAT) rand() / (FLOAT) RAND_MAX) - 0.5;
					fprintf(fp, FORMAT, a[(long)j + (long)i * (long)m * COMPSIZE]);
				}
			}
			fclose(fp);

			for (l=0; l<loops; l++)
			{
                                fp = fopen(FILEX,"w");
				for(i = 0; i < n * COMPSIZE * abs(inc_x); i++){
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
				gettimeofday( &start, (struct timezone *)0);
				GEMV (&trans, &m, &n, alpha, a, &m, x, &inc_x, beta, y, &inc_y );
				gettimeofday( &stop, (struct timezone *)0);
				time1 = (double)(stop.tv_sec - start.tv_sec) + (double)((stop.tv_usec - start.tv_usec)) * 1.e-6;
				timeg += time1;
                                fp = fopen(FILER,"w");
                                for (int q = 0; q < m * COMPSIZE * abs(inc_y); q++) //Check
                                {
	                           fprintf(fp, FORMAT, y[q]);
                                }
                                 fclose(fp);
			}

			timeg /= loops;

			fprintf(stderr, " %10.2f MFlops %10.6f sec\n", COMPSIZE * COMPSIZE * 2. * (double)m * (double)n / timeg * 1.e-6, timeg);
		}
		else{
			timeg=0;
			if ( has_param_n == 0 ) n = m;
			fprintf(stderr, " %6dx%d : ", (int)m,(int)n);
			fp = fopen(FILEA,"r");
			fp2 = fopen("new_sgemv_a","w");
			for(j = 0; j < m; j++){
				for(i = 0; i < n * COMPSIZE; i++){
					fscanf(fp, "%f\n", &a[(long)j + (long)i * (long)m * COMPSIZE]);
					fprintf(fp2, FORMAT, a[(long)j + (long)i * (long)m * COMPSIZE]);
				}
			}
			fclose(fp);
			fclose(fp2);

			for (l=0; l<loops; l++)
			{
                                fp = fopen(FILEX,"r");
				fp2 = fopen("new_sgemv_x","w");
				for(i = 0; i < n * COMPSIZE * abs(inc_x); i++){
					fscanf(fp, "%f\n", &x[i]);
					fprintf(fp2, FORMAT, x[i]);
				}
				fclose(fp);
				fclose(fp2);
                                fp = fopen(FILEY,"r");
				fp2 = fopen("new_sgemv_y","w");
				for(i = 0; i < m * COMPSIZE * abs(inc_y); i++){
					fscanf(fp, "%f\n", &y[i]);
					fprintf(fp2, FORMAT, y[i]);
				}
				fclose(fp);
				fclose(fp2);
				gettimeofday( &start, (struct timezone *)0);
				GEMV (&trans, &m, &n, alpha, a, &m, x, &inc_x, beta, y, &inc_y );
				gettimeofday( &stop, (struct timezone *)0);
				time1 = (double)(stop.tv_sec - start.tv_sec) + (double)((stop.tv_usec - start.tv_usec)) * 1.e-6;
				timeg += time1;
                                fp = fopen(FILER,"w");
                                for (int q = 0; q < m * COMPSIZE * abs(inc_y); q++) //Check
                                {
	                           fprintf(fp, FORMAT, y[q]);
                                }
                                 fclose(fp);
			}

			timeg /= loops;

			fprintf(stderr, " %10.2f MFlops %10.6f sec\n", COMPSIZE * COMPSIZE * 2. * (double)m * (double)n / timeg * 1.e-6, timeg);
		}
  	}
  }
  else
  {

  	for(n = from; n <= to; n += step)
  	{
 		if(random_input){
			timeg=0;
			if ( has_param_n == 0 ) n = m;
			fprintf(stderr, " %6dx%d : ", (int)m,(int)n);
			fp = fopen(FILEA,"w");
			
			for(j = 0; j < m; j++){
				for(i = 0; i < n * COMPSIZE; i++){
					a[(long)j + (long)i * (long)m * COMPSIZE] = ((FLOAT) rand() / (FLOAT) RAND_MAX) - 0.5;
					fprintf(fp, FORMAT, a[(long)j + (long)i * (long)m * COMPSIZE]);
				}
			}
			fclose(fp);

			for (l=0; l<loops; l++)
			{
                                fp = fopen(FILEX,"w");
				for(i = 0; i < n * COMPSIZE * abs(inc_x); i++){
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
				gettimeofday( &start, (struct timezone *)0);
				GEMV (&trans, &m, &n, alpha, a, &m, x, &inc_x, beta, y, &inc_y );
				gettimeofday( &stop, (struct timezone *)0);
				time1 = (double)(stop.tv_sec - start.tv_sec) + (double)((stop.tv_usec - start.tv_usec)) * 1.e-6;
				timeg += time1;
                                fp = fopen(FILER,"w");
                                for (int q = 0; q < m * COMPSIZE * abs(inc_y); q++) //Check
                                {
	                           fprintf(fp, FORMAT, y[q]);
                                }
                                 fclose(fp);
			}

			timeg /= loops;

			fprintf(stderr, " %10.2f MFlops %10.6f sec\n", COMPSIZE * COMPSIZE * 2. * (double)m * (double)n / timeg * 1.e-6, timeg);
		}
		else{
			timeg=0;
			if ( has_param_n == 0 ) n = m;
			fprintf(stderr, " %6dx%d : ", (int)m,(int)n);
			fp = fopen(FILEA,"r");
			fp2 = fopen("new_sgemv_a","w");
			for(j = 0; j < m; j++){
				for(i = 0; i < n * COMPSIZE; i++){
					fscanf(fp, "%f\n", &a[(long)j + (long)i * (long)m * COMPSIZE]);
					fprintf(fp2, FORMAT, a[(long)j + (long)i * (long)m * COMPSIZE]);
				}
			}
			fclose(fp);
			fclose(fp2);

			for (l=0; l<loops; l++)
			{
                                fp = fopen(FILEX,"r");
				fp2 = fopen("new_sgemv_x","w");
				for(i = 0; i < n * COMPSIZE * abs(inc_x); i++){
					fscanf(fp, "%f\n", &x[i]);
					fprintf(fp2, FORMAT, x[i]);
				}
				fclose(fp);
				fclose(fp2);
                                fp = fopen(FILEY,"r");
				fp2 = fopen("new_sgemv_y","w");
				for(i = 0; i < m * COMPSIZE * abs(inc_y); i++){
					fscanf(fp, "%f\n", &y[i]);
					fprintf(fp2, FORMAT, y[i]);
				}
				fclose(fp);
				fclose(fp2);
				gettimeofday( &start, (struct timezone *)0);
				GEMV (&trans, &m, &n, alpha, a, &m, x, &inc_x, beta, y, &inc_y );
				gettimeofday( &stop, (struct timezone *)0);
				time1 = (double)(stop.tv_sec - start.tv_sec) + (double)((stop.tv_usec - start.tv_usec)) * 1.e-6;
				timeg += time1;
                                fp = fopen(FILER,"w");
                                for (int q = 0; q < m * COMPSIZE * abs(inc_y); q++) //Check
                                {
	                           fprintf(fp, FORMAT, y[q]);
                                }
                                 fclose(fp);
			}

			timeg /= loops;

			fprintf(stderr, " %10.2f MFlops %10.6f sec\n", COMPSIZE * COMPSIZE * 2. * (double)m * (double)n / timeg * 1.e-6, timeg);
		}
  	}
  }

  return 0;
}

// void main(int argc, char *argv[]) __attribute__((weak, alias("MAIN__")));
