void matmul_sca(int* matA, int* matB, int val, int m, int n)
{
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      matB[i * n + j] = matA[i * n + j] * val;
    }
  }
}
