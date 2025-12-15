void n_real_updates(int N, int* A, int* B, int* C, int* D)
{
  int *p_a = &A[0], *p_b = &B[0];
  int *p_c = &C[0], *p_d = &D[0];
  int i;

  for (i = 0; i < N; i++)
    *p_d++ = *p_c++ + *p_a++ * *p_b++;
}
