void gemm(int* a, int* b, int* c, int M, int N, int K){
  for(int i = 0; i < M; i++)
    for(int j = 0; j < N; j++){
      a[i * M + j] = 0;
      for(int k = 0; k < K; k++)
        a[i * M + j] += b[i * M + k] * c[k * K + j];
    }
}
