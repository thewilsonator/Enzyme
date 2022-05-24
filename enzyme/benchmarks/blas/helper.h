#include <assert.h>
#include <stdlib.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

double r2() {
  return (double) rand() / (double) RAND_MAX ;
}

void randinit(double *v, int n) {
  for (int i = 0; i != n; ++i) {
    v[i] = r2();
  }
}

double sum(double *v, int n) {
  double s = 0;
  for (int i = 0; i != n; ++i) {
    s += v[i];
  }
  return s;
}

double sqnm(double *v, int n) {
  double s = 0;
  for (int i = 0; i != n; ++i) {
    s += v[i] * v[i];
  }
  return s;
}

void dlaswp(int n, double *a, int lda, int k1, int k2, int *ipiv, int incx) {
  int ix0, i1, i2, inc = 0;
  if (incx > 0) {
    ix0 = k1;
    i1 = k1;
    i2 = k2;
    inc = 1;
  } else if (incx < 0) {
    ix0 = k1 + (k1 - k2) * incx;
    i1 = k2;
    i2 = k1;
    inc = -1;
  } else {
    return;
  }
  int ix = ix0;
  for (int i = i1; i != i2; ++i) {
    int ip = ipiv[ix];
    for (int j = 0; j != n; ++j) {
      int tmp = a[j * lda + i];
      a[j * lda + i] = a[j * lda + ip];
      a[j * lda + ip] = tmp;
    }
    ix += incx;
  }
}
