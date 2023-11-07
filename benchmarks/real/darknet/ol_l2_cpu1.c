void ol_l2_cpu1(int n, int *pred, int *truth, int *error) {
  int i;
  for (i = 0; i < n; ++i) {
    int diff = truth[i] - pred[i];
    error[i] = diff * diff;
  }
}
