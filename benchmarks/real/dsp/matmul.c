void matmul(int* matA, int* matB, int* matC, int m, int n, int p)
{
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < p; ++j) {
      matC[p * i + j] = 0;
      for (int k = 0; k < n; ++k) {
        matC[p * i + j] += matA[n * i + k] * matB[p * k + j];
      }
    }
  }
}
