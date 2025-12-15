void cube_in_place(int* arr, int n)
{
  for (int i = 0; i < n; ++i) {
    arr[i] = arr[i] * arr[i] * arr[i];
  }
}
