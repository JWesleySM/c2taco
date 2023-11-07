void mult_add_into_cpu(int N, int *X, int *Y, int *Z)
{
    int i;
    for(i = 0; i < N; ++i) Z[i] += X[i]*Y[i];
}
