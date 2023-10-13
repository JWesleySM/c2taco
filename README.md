# C2TACO: Lifting Tensor Code to TACO

C2TACO is a lifting tool for synthesizing TACO, a well-known tensor DSL, from C code. We develop a guided enumerative synthesizer that uses automatically generated IO examples and program analyses to efficiently generate code. 

C2TACO takes as input a C function and performs source code analyses to retrieve features from the said function and use them as search aid during synthesis. Examples of features extracted are the number of tensor references and constants, the order of each tensor, and the binary operators present in the implemenetation. Using these features, C2TACO builds a reduced search space where the solution is more likely to be. We use a bottom-up enumerative synthesis algortihm to enumerate template TACO programs, i.e., TACO programs that use symbolic variables in place of all tensors and constants. We then check whether there is a valid substitution of inputs and constant literals for these symbolic variables that satisfies the specification. In our case, the specification consists of input-output examples.

In case the solution is not found in the initial search space, C2TACO then tries to use a simple version of enumerative template synthesis (refered here as ETS) to find the programs. ETS consider the whole space of programs of a given size and therefore is a slower process.

# Installation

Clone the C2TACO repository by:

```
$ git clone https://github.com/JWesleySM/c2taco
```
This will download the standard repository. A suite of tensor benchmarks is also available as a submodule. If you wish to use said suite, clone this repository using the command below:

```
$ git clone --recurse-submodules https://github.com/JWesleySM/c2taco
```

C2TACO's code analyses are implemented as Clang plugins. LLVM is necessary to build the libraries. You can either [download the binaries](https://releases.llvm.org/download.html#14.0.0) or [build it from source](https://llvm.org/docs/GettingStarted.html#getting-the-source-code-and-building-llvm). The analyses were implemented using Clang/LLVM version 14.0.0, so it is recommended to use that. Once you have LLVM installed, build the analyses by running the script provided (using bash) passing as argument the path to the `bin` directory of your LLVM installation:

```
$ ./build_code_analyses.sh <path-to-llvm-dir>
```

*NOTE:* in case you have built LLVM from source, the binaries will be located on the `build` directory.

To use C2TACO, you also need to install the following:

- TACO/PyTaco:

  C2TACO uses TACO [Python API](https://tensor-compiler.org/docs/pytensors.html) to check candidates during synthesis.

  * Follow the Build and test instructions in the TACO ![repository](https://github.com/tensor-compiler/taco)
  * *IMPORTANT:* build TACO enabling the Python API (-DPYTHON=ON)
  * After installing, make sure to include PyTaco in the Python path environment variable:
  ```
  $ export PYTHONPATH=<path-to-taco-repo>/build/lib:$PYTHONPATH
  ```

- SciPY:

  SciPY is needed by TACO's Python API. Install it via `pip`:

  ```
  $ pip install scipy
  ```

- exrex:

  C2TACO uses the exrex library to represent the grammar using regular expressions.

  * Install ![exrex library](https://github.com/asciimoo/exrex) via `pip`:

  ```
  $ pip install exrex
  ````
- clang:

  C2TACO uses the clang Python module to perform static analyses on programs.

  * Install clang python bindings via `pip` (tested against version 14):

  ```
  $ pip install clang==14
  ```
    
# Usage

Given an C code that performs some C tensor manipulations, C2TACO will look for an equivalent program in TACO. Correctness is done by checking behavioral equivalente. Therefore, input-output examples are also required as input to the synthesizer. IO files are specified in the JSON format. If you want to automatically generate IO samples for a C function, take a look at ![instructions](https://github.com/JWesleySM/c2taco/blob/main/io_gen/README.md). Run the synthesizer by providing both the original C implementation, the IO file, and the location of the Clang compiler as input:

```
$ python3 c2taco.py <path-to-c-program> <path-to-IO-samples> <path-to-clang-executable>
```

C2TACO will print the solution in the standard output, but it will also generate a lifting log at `<path-to-c-program>/name-of-c-program-lifting.log`. Log generation can be disabled by adding the `--no_log` option to the command above. 
The lifting log contains the following statistics of the synthesis execution:

* The solution itself
* Search space size, i.e., number of candidates in the search space considered
* Number of candidates tried
* Number of candidates discarded by type checking
* Number of candidates that produced runtime errors
* Time for parsing IO files
* Time for enumerate candidates
* Time for checking candidates
* Total synthesis time

If a solution is not found, the log is still produced. If C2TACO has to use ETS, that information will also appear in the log file. 

# Example

Consider the program below, taken from the UTDSP digital signal processing benchmark suite:

```c
1.  void mult_big(int A_ROW, int A_COL, int B_ROW, int B_COL, int* a_matrix,
2.     int* b_matrix, int* c_matrix){
3.   for (int i = 0; i < A_ROW; i++) {
4.     for (int j = 0; j < B_COL; j++) {
5.       int sum = 0.0;
6.       for (int k = 0; k < B_ROW; ++k) {
7.         sum += a_matrix[i * A_ROW + k] * b_matrix[k * B_ROW + j];
8.       }
9.       c_matrix[i * A_ROW + j] = sum;
10.    }
11.  }
12. }
```

C2TACO receives that function together with IO samples obtained by isolated executing `mult_big`. Then, it perform three different static code analyses on this program.

  * program length: this analysis will determine that there are 3 references for non-local tensor variables on the program, `a_matrix` and `b_matrix` at line 7 and `c_matrix` at line 9.
  * tensor orders/dimensions: by performing a combination of array recover and delinearization, this analysis points out that each of the tensors involded in the computation have order = 2, i.e., they are matrices.
  * operators: this analysis accounts for arithmetic operators in relevant computations in the program. For `mult_big`, there is a multiplication (`*`) operator at line 7.

Using the features extraced via the analyses, C2TACO starts the synthesis process searching specifically for TACO programs with 3 tensors, all of them having order = 2 and containing multiplication operators. The result of synthesis is a program written in the TACO index notation as shown below:

```
a(i,j) = b(i,k) * c(k,j)
```

which corresponds to matrix-matrix product. The program can then be passed as input to the TACO compiler, which can generate eihter high-performance C when targeting CPUs or CUDA code for GPUs. 

C version:
```c
int compute(taco_tensor_t *a, taco_tensor_t *b, taco_tensor_t *c) {
  int a1_dimension = (int)(a->dimensions[0]);
  int a2_dimension = (int)(a->dimensions[1]);
  int32_t* restrict a_vals = (int32_t*)(a->vals);
  int b1_dimension = (int)(b->dimensions[0]);
  int b2_dimension = (int)(b->dimensions[1]);
  int32_t* restrict b_vals = (int32_t*)(b->vals);
  int c1_dimension = (int)(c->dimensions[0]);
  int c2_dimension = (int)(c->dimensions[1]);
  int32_t* restrict c_vals = (int32_t*)(c->vals);

  #pragma omp parallel for schedule(static)
  for (int32_t pa = 0; pa < (a1_dimension * a2_dimension); pa++) {
    a_vals[pa] = 0;
  }

  #pragma omp parallel for schedule(runtime)
  for (int32_t i = 0; i < b1_dimension; i++) {
    for (int32_t k = 0; k < c1_dimension; k++) {
      int32_t kb = i * b2_dimension + k;
      for (int32_t j = 0; j < c2_dimension; j++) {
        int32_t ja = i * a2_dimension + j;
        int32_t jc = k * c2_dimension + j;
        a_vals[ja] = a_vals[ja] + b_vals[kb] * c_vals[jc];
      }
    }
  }
  return 0;
}

```
CUDA version:
```cuda
void computeDeviceKernel0(taco_tensor_t * __restrict__ a, taco_tensor_t * __restrict__ b, taco_tensor_t * __restrict__ c){
  int32_t i78 = blockIdx.x;
  int32_t i79 = (threadIdx.x % (256));
  if (threadIdx.x >= 256) {
    return;
  }

  int32_t i = i78 * 256 + i79;
  if (i >= b1_dimension)
    return;

  for (int32_t k = 0; k < c1_dimension; k++) {
    int32_t kb = i * b2_dimension + k;
    for (int32_t j = 0; j < c2_dimension; j++) {
      int32_t ja = i * a2_dimension + j;
      int32_t jc = k * c2_dimension + j;
      a_vals[ja] = a_vals[ja] + b_vals[kb] * c_vals[jc];
    }
  }
}
```
