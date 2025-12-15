void lerp(int* out, int* x, int* y, int alpha, int n)
{
  for (int i = 0; i < n; ++i) {
    out[i] = alpha * x[i] + (1 - alpha) * y[i];
  }
}
