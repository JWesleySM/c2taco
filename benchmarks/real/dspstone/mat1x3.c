void mat1x3(int N, int* h, int* x, int* y)
{
  int* p_x;
  int* p_h;
  int* p_y;

  int f, i;

  p_h = h;
  p_y = y;

  for (i = 0; i < N; i++) {
    *p_y = 0;
    p_x = &x[0];


    for (f = 0; f < N; f++)
      *p_y += *p_h++ * *p_x++;

    p_y++;
  }
}
