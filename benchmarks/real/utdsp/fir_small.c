int fir_small(int NTAPS, int* input, int* coefficient)
{
  int sum = 0;

  for (int i = 0; i < NTAPS; ++i) {
    sum += input[i] * coefficient[i];
  }

  return sum;
}
