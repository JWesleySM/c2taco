void ol_l2_cpu2(int n, int *pred, int *truth, int *delta) {
  int i;
  for (i = 0; i < n; ++i) {
    int diff = truth[i] - pred[i];
    delta[i] = diff;
  }
}
