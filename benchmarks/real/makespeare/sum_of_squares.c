
int sum_of_squares(int* arr, int n)
{
  int sum = 0;
  for (int i = 0; i < n; ++i) {
    sum += arr[i] * arr[i];
  }
  return sum;
}
