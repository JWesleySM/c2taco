void gemv(int M, int N, int* A, int* x, int* y)
{
  for (int i = 0; i < M; ++i) {
    int sum = 0;
    for (int j = 0; j < N; ++j) {
      sum += A[j + i * N] * x[j];
    }
    y[i] = sum;
  }
}
