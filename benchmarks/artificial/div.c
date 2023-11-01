void div(int* in1, int* in2, int* out, int len){
  for(int i = 0; i < len; i++)
    out[i] = in1[i] / in2[i];
}