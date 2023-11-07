typedef struct matrix{
  int rows, cols;
  int **vals;
}matrix;

void matrix_add_matrix(matrix from, matrix to)
{
    int i,j;
    for(i = 0; i < from.rows; ++i){
        for(j = 0; j < from.cols; ++j){
            to.vals[i][j] += from.vals[i][j];
        }
    }
}
