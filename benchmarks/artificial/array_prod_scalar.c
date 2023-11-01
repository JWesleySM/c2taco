void array_prod_scalar(int* a, int b, int *c, int len){
  for(int i = 0; i < len; i ++)
    a[i] = c[i] * b;
}
