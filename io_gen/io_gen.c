#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void fill_array(int* arr, int len){
  for(int i = 0; i < len; i++)
    arr[i] = 1 + rand() % 100;
}

void print_array(const char* name, int* arr, int len){
  printf("%s: %d: ", name, len);
  for(int i = 0; i < len; i++)
    printf("%d ", arr[i]);
  printf("\n");
}

typedef struct{
  int rows;
  int cols;
  int** vals;
}matrix;


matrix* allocate_darknet_matrix(int rows, int cols){
  matrix* m = (matrix*)malloc(sizeof(matrix));
  m->rows = rows;
  m->cols = cols;
  m->vals = (int**)malloc(m->rows * sizeof(int*));
  for(int i = 0; i < rows; i++)
    m->vals[i] = (int*)malloc(m->cols * sizeof(int));

  return m;
}

void fill_darknet_matrix(matrix* m){
  for(int i = 0; i < m->rows; i++){
    fill_array(m->vals[i], m->cols);
  }
}

void print_darknet_matrix(const char* name, matrix m){
  printf("%s: %d: ", name, m.rows * m.cols);
  for(int i = 0; i < m.rows; i++){
    for(int j = 0; j< m.cols; j++){
      printf("%d ", m.vals[i][j]);
    }
  }
  printf("\n");
}

void copy_darknet_matrix(matrix* from, matrix* to){
  for(int i = 0; i < from->rows; i++){
    for(int j = 0; j< from->cols; j++){
      to->vals[i][j] = from->vals[i][j];
    }
  }
}

