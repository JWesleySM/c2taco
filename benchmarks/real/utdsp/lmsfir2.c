void lmsfir2(
    int NTAPS, int* input, int* output, int* expected,
    int* coefficient, int gain, int sum, int error)
{
  
  for (int i = 0; i < NTAPS - 1; ++i) {
    coefficient[i] += input[i] * error;
  }
}
