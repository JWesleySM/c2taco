int sum_array(int *a, int n)
{
    int i;
    int sum = 0;
    for(i = 0; i < n; ++i) sum += a[i];
    return sum;
}
