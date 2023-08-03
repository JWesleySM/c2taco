from clang.cindex import Index, CursorKind, TypeKind
import json
import os

ARRAYS_TYPES = [TypeKind.POINTER, TypeKind.CONSTANTARRAY, TypeKind.VARIABLEARRAY, 
               TypeKind.INCOMPLETEARRAY, TypeKind.DEPENDENTSIZEDARRAY, 
               TypeKind.TYPEDEF]
SCALAR_TYPES = [TypeKind.BOOL, TypeKind.CHAR_U, TypeKind.UCHAR, TypeKind.CHAR16,
                TypeKind.CHAR32, TypeKind.USHORT, TypeKind.UINT, TypeKind.ULONG,
                TypeKind.ULONGLONG, TypeKind.UINT128, TypeKind.CHAR_S, TypeKind.SCHAR,
                TypeKind.WCHAR, TypeKind.WCHAR, TypeKind.SHORT, TypeKind.INT,
                TypeKind.LONG, TypeKind.LONGLONG, TypeKind.INT128, TypeKind.FLOAT,
                TypeKind.DOUBLE, TypeKind.LONGDOUBLE]

def is_array(var_type):
  return True if var_type.kind in ARRAYS_TYPES else False


def is_scalar(var_type):
  return True if var_type.kind in SCALAR_TYPES else False


def get_output(output_var, args):
  output = None
  for arg in args:
    if arg.spelling == output_var:
      output = arg
      break

  assert output, 'Please indicate the output variable'
  return output


def get_kernel_signature(benchmark_path):
  kernel_name = ''
  return_type = None
  
  idx = Index.create()
  tu = idx.parse(benchmark_path, args = ['-c'])
  for n in tu.cursor.walk_preorder():
    if n.kind == CursorKind.FUNCTION_DECL:
      kernel_name = n.spelling
      return_type = n.result_type
      args = list(n.get_arguments())
      break

  return kernel_name, return_type, args


def get_preamble():
  return '#include <stdlib.h>\n\
#include <stdio.h>\n\
#include <time.h>\n\
extern void fill_array(int* arr, int len);\n\
extern void print_array(const char* name, int* arr, int len);\n\n'


def get_print_input_func(args, value_profile):
  header = 'void print_inputs('
  body = '  printf(\"input\\n\");\n'
  for arg in args:
    if is_array(arg.type):
      header += 'int* '
      body += f'  print_array(\"{arg.spelling}\", {arg.spelling}, {value_profile[arg.spelling]});\n'
    elif is_scalar(arg.type):
      header += 'int '
      body += f'  printf(\"{arg.spelling}: 1: %d\\n\", {arg.spelling});\n'

    header += f'{arg.spelling}, '
    
  header = header.rstrip(', ') + '){\n'
  body += '}\n\n'
  return header + body


def get_print_output_func(print_return_value, output, value_profile):
  header = 'void print_output(int sample_id, '
  body = '  printf(\"output\\n\");\n'
  if print_return_value:
    header += 'int returnv'
    body += '  printf(\"returnv :1 :%d\\n\", returnv);\n'
  else:
    if is_array(output.type):
      header += 'int* '
      body += f'  print_array(\"{output.spelling}\", {output.spelling}, {value_profile[output.spelling]});\n'
    elif is_scalar(output.type):
      header += 'int '
      body += f'  printf(\"{output.spelling}: 1: %d\\n\", {output.spelling});\n'
    header += f'{output.spelling}'

  header += '){\n'
  body += '  printf(\"sample_id %d\\n\", sample_id);\n}\n'
  return header + body


def get_main_func(kernel, return_type, args, output_var, value_profile):
  main_opening = '\nint main(int argc, char* argv[]){\n\
  srand(time(0));\n\
  int n_io = atoi(argv[1]);\n'
  main_for='\n  for(int i = 0; i < n_io; i++){\n'
  args_initialization = ''
  array_initializations = ''
  kernel_call = f'    {kernel}(' if return_type.kind == TypeKind.VOID else f'    int returnv = {kernel}('
  print_inputs_call = '    print_inputs('
  for arg in args:
    if is_array(arg.type):
      args_initialization += f'  int* {arg.spelling} = (int*)malloc({value_profile[arg.spelling]} * sizeof(int));\n'
      array_initializations += f'    fill_array({arg.spelling}, {value_profile[arg.spelling]});\n'
    elif is_scalar(arg.type):
      args_initialization += f'  int {arg.spelling} = {value_profile[arg.spelling]};\n'

    kernel_call += f'{arg.spelling}, '
    print_inputs_call += f'{arg.spelling}, '

  kernel_call = kernel_call.rstrip(', ') + ');\n'
  print_inputs_call = print_inputs_call.rstrip(', ') + ');\n'
  print_output_call = '    print_output(i, '
  print_output_call = print_output_call + output_var + ');\n' if return_type.kind == TypeKind.VOID else print_output_call + 'returnv);\n'
  main_closing = '  }\n  return 0;\n}'
  return main_opening + args_initialization + main_for + array_initializations + print_inputs_call + kernel_call + print_output_call + main_closing


def write_driver(benchmark_path, benchmark, output_var, value_profile):
  with open(os.path.join(os.path.dirname(benchmark_path), f'main_{benchmark}.c'), 'w') as driver_file:
    driver_file.write(get_preamble())
    kernel, return_type, args = get_kernel_signature(benchmark_path)
    driver_file.write(get_print_input_func(args, value_profile))
    print_return_value = False if return_type.kind == TypeKind.VOID else True
    output = get_output(output_var, args) if not print_return_value else None
    driver_file.write(get_print_output_func(print_return_value, output, value_profile))
    driver_file.write(get_main_func(kernel, return_type, args, output_var, value_profile))


def read_value_profile(value_profile):
  value_profile_dict = dict()
  with open(value_profile, 'r') as value_profile_file:
    value_profile_dict = json.load(value_profile_file)
  
  return value_profile_dict


def gen_driver(benchmark_path, output_var, value_profile_file):
  benchmark = os.path.basename(benchmark_path)[:-2]
  value_profile = read_value_profile(value_profile_file)
  write_driver(benchmark_path, benchmark, output_var, value_profile)
  
