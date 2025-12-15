void w_vec(int* a, int* b, int m, int* c, int n)
{
  for (int i = 0; i < n; ++i) {
    c[i] = m * a[i] + b[i];
  }
}
