typedef struct matrix{
  int rows, cols;
  int **vals;
}matrix;

void scale_matrix_int(matrix m, int scale)
{
    int i,j;
    for(i = 0; i < m.rows; ++i){
        for(j = 0; j < m.cols; ++j){
            m.vals[i][j] *= scale;
        }
    }
}
