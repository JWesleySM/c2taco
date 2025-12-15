void matrix1(int X, int Y, int Z, int* A, int* B, int* C)
{
  int* p_a = &A[0];
  int* p_b = &B[0];
  int* p_c = &C[0];

  int i, f;
  int k;

  for (k = 0; k < Z; k++) {
    p_a = &A[0]; 

    for (i = 0; i < X; i++) {
      p_b = &B[k * Y]; 

      *p_c = 0;

      for (f = 0; f < Y; f++) 
        *p_c += *p_a++ * *p_b++;

      (void)*p_c++;
    }
  }
}
