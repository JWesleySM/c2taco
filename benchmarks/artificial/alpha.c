void alpha(int* in1, int* in2, int* out, int n, int m, int len){
  int alpha = n * m;
  for(int i = 0; i < len; i++)
    out[i] = in1[i] + alpha * in2[i];
}