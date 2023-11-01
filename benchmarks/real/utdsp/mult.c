void mult(
    int A_ROW, int A_COL, int B_ROW, int B_COL, int C_ROW, int C_COL, int a_matrix[A_ROW][A_COL],
    int b_matrix[B_ROW][B_COL], int c_matrix[C_ROW][C_COL])
{
  int i, j, k;
  int sum;

  for (i = 0; i < A_ROW; i++) {
    for (j = 0; j < B_COL; j++) {
      sum = 0;
      for (k = 0; k < B_ROW; ++k) {
        sum += a_matrix[i][k] * b_matrix[k][j];
      }
      c_matrix[i][j] = sum;
    }
  }
}
