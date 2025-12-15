int len(int* arr, int n)
{
  int l = 0;
  for (int i = 0; i < n; ++i) {
    l += arr[i] * arr[i];
  }
  return l;
}
