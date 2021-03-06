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


#undef GEMM

#ifndef COMPLEX

#ifdef DOUBLE
#define GEMM   BLASFUNC(dgemm)
#define FILEA   "dgemm_a.txt"
#define FILEB   "dgemm_b.txt"
#define FILEC   "dgemm_c.txt"
#define FILER   "dgemm_res.txt"
#define FORMAT  "%lf\n"
#elif defined(HALF)
#define GEMM   BLASFUNC(shgemm)
#define FILEA   "shgemm_a.txt"
#define FILEB   "shgemm_b.txt"
#define FILEC   "shgemm_c.txt"
#define FILER   "shgemm_res.txt"
#define FORMAT  "%.14f\n"
#else
#define GEMM   BLASFUNC(sgemm)
#define FILEA   "sgemm_a.txt"
#define FILEB   "sgemm_b.txt"
#define FILEC   "sgemm_c.txt"
#define FILER   "sgemm_res.txt"
#define FORMAT  "%.14f\n"
#endif

#else

#ifdef DOUBLE
#define GEMM   BLASFUNC(zgemm)
#define FILEA   "zgemm_a.txt"
#define FILEB   "zgemm_b.txt"
#define FILEC   "zgemm_c.txt"
#define FILER   "zgemm_res.txt"
#define FORMAT  "%lf\n"
#else
#define GEMM   BLASFUNC(cgemm)
#define FILEA   "cgemm_a.txt"
#define FILEB   "cgemm_b.txt"
#define FILEC   "cgemm_c.txt"
#define FILER   "cgemm_res.txt"
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

  IFLOAT *a, *b;
  FLOAT *c;
  FLOAT alpha[] = {1.0, 0.0};
  FLOAT beta [] = {0.0, 0.0};
  char transa = 'N';
  char transb = 'N';
  blasint m, n, k, i, j, lda, ldb, ldc;
  int loops = 1;
  int has_param_m = 0;
  int has_param_n = 0;
  int has_param_k = 0;
  char *p;

  int from =   1;
  int to   = 200;
  int step =   1;
  int random_input = 0; //Varun added

  struct timeval start, stop;
  double time1, timeg;

  argc--;argv++;
  if (argc > 0) { random_input = atol(*argv);    argc--; argv++; }
  if (argc > 0) { from = atol(*argv);            argc--; argv++; }
  if (argc > 0) { to   = MAX(atol(*argv), from); argc--; argv++; }
  if (argc > 0) { step = atol(*argv);            argc--; argv++; }

  if ((p = getenv("OPENBLAS_TRANS"))) {
    transa=*p;
    transb=*p;
  }
  if ((p = getenv("OPENBLAS_TRANSA"))) {
    transa=*p;
  }
  if ((p = getenv("OPENBLAS_TRANSB"))) {
    transb=*p;
  }
  TOUPPER(transa);
  TOUPPER(transb);

  fprintf(stderr, "From : %3d  To : %3d Step=%d : Transa=%c : Transb=%c\n", from, to, step, transa, transb);

  p = getenv("OPENBLAS_LOOPS");
  if ( p != NULL ) {
    loops = atoi(p);
  }

  if ((p = getenv("OPENBLAS_PARAM_M"))) {
    m = atoi(p);
    has_param_m=1;
  } else {
    m = to;
  }
  if ((p = getenv("OPENBLAS_PARAM_N"))) {
    n = atoi(p);
    has_param_n=1;
  } else {
    n = to;
  }
  if ((p = getenv("OPENBLAS_PARAM_K"))) {
    k = atoi(p);
    has_param_k=1;
  } else {
    k = to;
  }

  if (( a = (IFLOAT *)malloc(sizeof(IFLOAT) * m * k * COMPSIZE)) == NULL) {
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }
  if (( b = (IFLOAT *)malloc(sizeof(IFLOAT) * k * n * COMPSIZE)) == NULL) {
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }
  if (( c = (FLOAT *)malloc(sizeof(FLOAT) * m * n * COMPSIZE)) == NULL) {
    fprintf(stderr,"Out of Memory!!\n");exit(1);
  }

#ifdef linux
  srandom(getpid());
#endif
  FILE *fp;
  FILE *fp2;
  if(random_input){
	  fp = fopen(FILEA,"w");
	  for (i = 0; i < m * k * COMPSIZE; i++) {
	    a[i] = ((IFLOAT) rand() / (IFLOAT) RAND_MAX) - 0.5;
	    fprintf(fp, FORMAT, a[i]);
	  }
	  fclose(fp);
	  fp = fopen(FILEB,"w");
	  for (i = 0; i < k * n * COMPSIZE; i++) {
	    b[i] = ((IFLOAT) rand() / (IFLOAT) RAND_MAX) - 0.5;
	    fprintf(fp, FORMAT, b[i]);
	  }
	  fclose(fp);
	  fp = fopen(FILEC,"w");
	  for (i = 0; i < m * n * COMPSIZE; i++) {
	    c[i] = ((FLOAT) rand() / (FLOAT) RAND_MAX) - 0.5;
	    fprintf(fp, FORMAT, c[i]);
	  }
	  fclose(fp);
  }
  else{
	  fp = fopen(FILEA,"r");
	  fp2 = fopen("new_a.txt","w");
	  for (i = 0; i < m * k * COMPSIZE; i++) {
	    fscanf(fp, "%f\n", &a[i]);
            fprintf(fp2, FORMAT, a[i]);
// 	    printf("%f", a[i]);
	  }
	  fclose(fp2);
	  fclose(fp);
	  fp = fopen(FILEB,"r");
	  fp2 = fopen("new_b.txt","w");
	  for (i = 0; i < k * n * COMPSIZE; i++) {
	    fscanf(fp, "%f\n", &b[i]);
	    fprintf(fp2, FORMAT, b[i]);
	  }
	  fclose(fp2);
	  fclose(fp);
	  fp = fopen(FILEC,"r");
	  fp2 = fopen("new_c.txt","w");
	  for (i = 0; i < m * n * COMPSIZE; i++) {
	     fscanf(fp, "%f\n", &c[i]);
	     fprintf(fp2, FORMAT, c[i]);
	  }
	  fclose(fp2);
	  fclose(fp);
  }

  fprintf(stderr, "          SIZE                   Flops             Time\n");

  for (i = from; i <= to; i += step) {
    
    timeg=0;

    if (!has_param_m) { m = i; }
    if (!has_param_n) { n = i; }
    if (!has_param_k) { k = i; }

    if (transa == 'N') { lda = m; }
    else { lda = k; }
    if (transb == 'N') { ldb = k; }
    else { ldb = n; }
    ldc = m;

    fprintf(stderr, " M=%4d, N=%4d, K=%4d : ", (int)m, (int)n, (int)k);
    gettimeofday( &start, (struct timezone *)0);

    for (j=0; j<loops; j++) {
      GEMM (&transa, &transb, &m, &n, &k, alpha, a, &lda, b, &ldb, beta, c, &ldc);
      fp = fopen(FILER,"w");
      for (int x = 0; x < m * n * COMPSIZE; x++) 
      {
	 fprintf(fp, FORMAT, c[x]);
      }
      fclose(fp);
	    
    }

    gettimeofday( &stop, (struct timezone *)0);
    time1 = (double)(stop.tv_sec - start.tv_sec) + (double)((stop.tv_usec - start.tv_usec)) * 1.e-6;

    timeg = time1/loops;
    fprintf(stderr,
	    " %10.2f MFlops %10.6f sec\n",
	    COMPSIZE * COMPSIZE * 2. * (double)k * (double)m * (double)n / timeg * 1.e-6, time1);
    
  }

  return 0;
}

// void main(int argc, char *argv[]) __attribute__((weak, alias("MAIN__")));
