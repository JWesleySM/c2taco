void matadd(int* matA, int* matB, int m, int n)
{
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      matA[i * n + j] += matB[i * n + j];
    }
  }
}
