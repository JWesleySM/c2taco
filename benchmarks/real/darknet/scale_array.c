void scale_array(int *a, int n, int s)
{
    int i;
    for(i = 0; i < n; ++i){
        a[i] *= s;
    }
}
