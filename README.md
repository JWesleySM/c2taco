# C2TACO: Lifting Tensor Code to TACO

C2TACO is a lifting tool for synthesizing TACO, a well-known tensor DSL, from C code. We develop a smart, enumerative synthesizer that uses automatically generated IO examples and program analyses to efficiently generate code. 

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

To use C2TACO, you also need to install the following:

- TACO/PyTaco:

  C2TACO uses TACO ![Python API](https://tensor-compiler.org/docs/pytensors.html) to check candidates during synthesis.

  * Follow the Build and test instructions in the TACO ![repository](https://github.com/tensor-compiler/taco)
  * *IMPORTANT:* build TACO enabling the Python API (-DPYTHON=ON)
  * After installing, make sure to include PyTaco in the Python path environment variable:
  ```
  $ export PYTHONPATH=<path-to-taco-repo>/build/lib:$PYTHONPATH
  ```

- exrex:

  C2TACO uses the exrex library to represent the grammar using regular expressions.

  * Install ![exrex library](https://github.com/asciimoo/exrex) via `pip`:

  ```
  $ pip install exrex
  ````
- clang:

  C2TACO uses the clang Python module to perform static analyses on programs.

  * Install clang python bindings via `pip`:

  ```
  $ pip install clang
  ```
    
# Usage

Given an C code that performs some C tensor manipulations, C2TACO will look for an equivalent program in TACO. Correctness is done by checking behavioral equivalente. Therefore, input-output examples are also required as input to the synthesizer. IO files are specified in the JSON format. If you want to automatically generate IO samples for a C function, take a look at ![instructions](https://github.com/JWesleySM/c2taco/blob/main/io_gen/README.md). Run the synthesizer by providing both the original C implementation and the IO file as input:

```
$ python3 main.py <path-to-c-program> <path-to-IO-samples>
```

C2TACO will print the solution in the standard output, but it will also generate a log at `<path-to-c-program>/name-of-c-program-lifting.log`. This log contains the following statistics of the synthesis execution:

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
WIP
