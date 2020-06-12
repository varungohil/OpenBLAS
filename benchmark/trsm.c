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


#undef TRSM

#ifndef COMPLEX

#ifdef DOUBLE
#define TRSM   BLASFUNC(dtrsm)
#define FILEA   "dtrsm_a.txt"
#define FILEB   "dtrsm_b.txt"
#define FILER   "dtrsm_res.txt"
#define FORMAT  "%lf\n"
#else
#define TRSM   BLASFUNC(strsm)
#define FILEA   "strsm_a.txt"
#define FILEB   "strsm_b.txt"
#define FILER   "strsm_res.txt"
#define FORMAT  "%.14f\n"
#endif

#else

#ifdef DOUBLE
#define TRSM   BLASFUNC(ztrsm)
#define FILEA   "ztrsm_a.txt"
#define FILEB   "ztrsm_b.txt"
#define FILER   "ztrsm_res.txt"
#define FORMAT  "%lf\n"
#else
#define TRSM   BLASFUNC(ctrsm)
#define FILEA   "ctrsm_a.txt"
#define FILEB   "ctrsm_b.txt"
#define FILER   "ctrsm_res.txt"
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

  FLOAT *a, *b;
  FLOAT alpha[] = {1.0, 1.0};
  FLOAT beta [] = {1.0, 1.0};
  char *p;

  char side ='L';
  char uplo ='U';
  char trans='N';
  char diag ='U';


  int l;
  int loops = 1;
  double timeg;

  if ((p = getenv("OPENBLAS_SIDE"))) side=*p; 
  if ((p = getenv("OPENBLAS_UPLO"))) uplo=*p;
  if ((p = getenv("OPENBLAS_TRANS"))) trans=*p;
  if ((p = getenv("OPENBLAS_DIAG"))) diag=*p;

  p = getenv("OPENBLAS_LOOPS");
  if ( p != NULL )
        loops = atoi(p);


  blasint m, i, j;

  int from =   1;
  int to   = 200;
  int step =   1;
  int random_input = 0; //Varun added

  struct timeval start, stop;
  double time1;

  argc--;argv++;
  if (argc > 0) { random_input = atol(*argv);    argc--; argv++; }
  if (argc > 0) { from     = atol(*argv);		argc--; argv++;}
  if (argc > 0) { to       = MAX(atol(*argv), from);	argc--; argv++;}
  if (argc > 0) { step     = atol(*argv);		argc--; argv++;}

  fprintf(stderr, "From : %3d  To : %3d Step = %3d Side = %c Uplo = %c Trans = %c Diag = %c Loops = %d\n", from, to, step,side,uplo,trans,diag,loops);

  if (( a = (FLOAT *)malloc(sizeof(FLOAT) * to * to * COMPSIZE)) == NULL){
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }

  if (( b = (FLOAT *)malloc(sizeof(FLOAT) * to * to * COMPSIZE)) == NULL){
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }



#ifdef linux
  srandom(getpid());
#endif
  FILE *fpa;
  FILE *fpb;
  FILE *fp1;
  FILE *fp2;
  fprintf(stderr, "   SIZE       Flops\n");

  for(m = from; m <= to; m += step)
  {

	timeg=0.0;

        fprintf(stderr, " %6d : ", (int)m);

	for (l=0; l<loops; l++)
    	{
		if(random_input)
		{
		         fpa = fopen(FILEA,"w");
			 fpb = fopen(FILEB,"w");
			 for(j = 0; j < m; j++){
				for(i = 0; i < m * COMPSIZE; i++){
					a[(long)i + (long)j * (long)m * COMPSIZE] = ((FLOAT) rand() / (FLOAT) RAND_MAX) - 0.5;
					b[(long)i + (long)j * (long)m * COMPSIZE] = ((FLOAT) rand() / (FLOAT) RAND_MAX) - 0.5;
					fprintf(fpa, FORMAT, a[(long)i + (long)j * (long)m * COMPSIZE]);
					fprintf(fpb, FORMAT, b[(long)i + (long)j * (long)m * COMPSIZE]);
				}
			 }
		         fclose(fpa);
			 fclose(fpb);

			gettimeofday( &start, (struct timezone *)0);

			TRSM (&side, &uplo, &trans, &diag, &m, &m, alpha, a, &m, b, &m);

			gettimeofday( &stop, (struct timezone *)0);
			fpb = fopen(FILER,"w");
			 for(j = 0; j < m; j++){
				for(i = 0; i < m * COMPSIZE; i++){
					fprintf(fpb, FORMAT, b[(long)i + (long)j * (long)m * COMPSIZE]);
				}
			 }
		         fclose(fpb);
		}
		else
		{
		         fpa = fopen(FILEA,"r");
			 fpb = fopen(FILEB,"r");
		         fp1 = fopen(FILEA,"w");
			 fp2 = fopen(FILEB,"w");
			 for(j = 0; j < m; j++){
				for(i = 0; i < m * COMPSIZE; i++){
					fscanf(fpa, "%f\n", &a[(long)i + (long)j * (long)m * COMPSIZE]);
					fscanf(fpb, "%f\n", &b[(long)i + (long)j * (long)m * COMPSIZE]);
					fprintf(fp1, FORMAT, a[(long)i + (long)j * (long)m * COMPSIZE]);
					fprintf(fp2, FORMAT, b[(long)i + (long)j * (long)m * COMPSIZE]);
				}
			 }
		         fclose(fpa);
			 fclose(fpb);
		         fclose(fp1);
			 fclose(fp2);
			
			gettimeofday( &start, (struct timezone *)0);

			TRSM (&side, &uplo, &trans, &diag, &m, &m, alpha, a, &m, b, &m);

			gettimeofday( &stop, (struct timezone *)0);
			fpb = fopen(FILER,"w");
			 for(j = 0; j < m; j++){
				for(i = 0; i < m * COMPSIZE; i++){
					fprintf(fpb, FORMAT, b[(long)i + (long)j * (long)m * COMPSIZE]);
				}
			 }
		         fclose(fpb);
		}
    		time1 = (double)(stop.tv_sec - start.tv_sec) + (double)((stop.tv_usec - start.tv_usec)) * 1.e-6;

		timeg += time1;
        }

	time1 = timeg/loops;

        fprintf(stderr, " %10.2f MFlops\n", COMPSIZE * COMPSIZE * 1. * (double)m * (double)m * (double)m / time1 * 1.e-6);

  }

  return 0;
}

// void main(int argc, char *argv[]) __attribute__((weak, alias("MAIN__")));
