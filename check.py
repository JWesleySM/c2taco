from contextlib import redirect_stdout
from enum import Enum
from io import StringIO
import itertools
from math import ceil
import re

class CheckingReturnCode(Enum):
  """A enumeration to describe the possible results of checking a candidate.
   - SUCCESS: the candidate produces the expected output for all the input samples
   - TYPE_DISCARDED: the candidate was discarded because it does not type-check agains the IO
   - RUNTIEM_ERROR: there was a runtime error while executing the candidate
   - CANDIDATE_TRIED: the candidate was succesfully executed, but it does not produce the
                      correct output in some of the IO samples.
  """
  SUCCESS = 0
  TYPE_DISCARDED = 1
  RUNTIME_ERROR = 2
  CANDIDATE_TRIED = 3


class InsufficientElements(Exception):
  """An excepetion to indicate that there are no enough elements for a candidate in
  an IO sample.
  """
  pass


def get_tensor_order(t):
  """Returns the order of a given tensor leveraging its string representation."""
  return 0 if t.count('(') == 0 else t.count(',') + 1


def is_scalar(t):
  """Check if a tensor is a scalar, i.e., it has order 0."""
  return get_tensor_order(t) == 0


def is_compatible(tensor_order, values):
  """Check if a tensor can be assigned to a list of values.
  The input value cannot be a single number if the tensor
  has order bigger than 0. Analogously, the value must be a single
  number if the tensor has order 0.
  """
  if tensor_order > 0 and len(values) == 1:
    return False
  if tensor_order == 0 and len(values) > 1:
    return False

  return True


def is_io_compatible(c, io):
  """Check if the candidate is unsuitable given the shape of the io"""
  # Regarding output, its value cannot be a single number if the tensor
  # has order bigger than 0. Analogously the output value must be a single
  # number if the output tensor has order 0
  if not is_compatible(c.get_order(c.get_lhs()), io.output.values):
    return False

  # A candidate can be typed discarded given an IO sample if
  #  1. number of tensors with order bigger than 0 > number of inputs which are lists

  tensor_orders = c.get_tensor_orders()
  n_scalars_candidate = sum(1 for ord in tensor_orders[1:] if ord == 0)
  n_scalars_io = sum(1 for var in io.input.values() if len(var.values) == 1)
  if n_scalars_candidate > 0 and n_scalars_io == 0:
    return False
  
  #  2. number of tensors with order 0 > number of inputs which are a single integer.
  n_non_scalars_candidate = len(c.get_tensors()[1:]) - n_scalars_candidate
  n_non_scalars_io = len(io.input) - n_scalars_io
  if n_non_scalars_candidate > 0 and n_non_scalars_io == 0:
    return False

  return True


def is_valid_binding(binding, inputs, candidate):
  """This function checks if a substitution is valid, i.e., if the 
  mappings input variable -> tensor are type-valid.
  """
  bond = dict()
  for input_var, tensor in binding:
    # Constant tensors can only be bond to constant values.
    if tensor.startswith('Cons'):
      if not input_var.startswith('Cons'):
        return False
    # In case of non-constant tensors, we need to check type compatibility.
    else:
      if input_var.startswith('Cons'):
        return False
      elif not is_compatible(candidate.get_order(tensor), inputs[input_var].values):
        return False
    
    # A same tensor cannot be bond to two different inputs.
    if tensor in bond:
      if bond[tensor] != input_var:
        return False
    bond[tensor] = input_var

  return True


def get_bindings_permutation(candidate, io_sample):
  """Return all the possible substitutions for a candidate program
  given an IO sample.
  """
  # We only need to bind input variables to unique references in the
  # program, hence, a set is used.
  tensors = set(candidate.get_tensors()[1:])
  taco_input_perm = []
  input_list = dict(**io_sample.input,  **io_sample.constants) if candidate.has_constant() else io_sample.input
  for p in itertools.permutations(input_list.keys(), len(tensors)):
    input_combs = list(zip(p, tensors))
    if is_valid_binding(input_combs, input_list, candidate):      
      taco_input_perm.append(input_combs)

  return taco_input_perm 


def build_env(lhs, lhs_order, binding, io):
  """Builds an enviroment for free variables to create a PyTaco program."""
  env = dict()
  env[lhs] = (1, [0]) if lhs_order == 0 else (io.output.dimension, [0] * io.output.dimension)
  for input_var, tensor in binding:
    if tensor.startswith('Cons'):
      env[tensor] = (1, io.constants[input_var])
    else:
      env[tensor] = (io.input[input_var].dimension, io.input[input_var].values)
  
  return env


def write_pytaco_program(candidate, env):
  """Write a program using PyTACO, TACO's Python API."""
  # The tensors in PyTaco must be declared with fixed dimension lengths.
  # We determine how the elements will be distributed by computing the nth 
  # root of the number of elements, where 'n' is the order of the tensor.
  tensors = candidate.get_tensors()
  defined = dict([(t, False) for t in tensors])
  # Omport PyTaco and NumPy.
  imports = 'import pytaco as pt\nimport numpy as np\n'
  # Declare tensors.
  t_declarations = ''
  t_initializations = ''
  for t in tensors:
    if defined[t]:
      continue
    order = candidate.get_order(t)
    t_declarations += f'{t} = '
    if order == 0:
      # Constants are declared as TACO tensors to keep the computation format uniform.
      if t.startswith('Cons'):
        t_declarations += f'pt.tensor({env[t][1]}, dtype = pt.int32)\n'
      else:
        t_declarations += f'pt.tensor({env[t][1][0]}, dtype = pt.int32)\n'
      defined[t] = True
      continue
    else:
      elements_by_dimension = ceil(env[t][0] ** (1/order)) if order > 0 else 1
      if elements_by_dimension ** order > len(env[t][1]):
        raise InsufficientElements(f'Not enough elements for tensor {t} (needs {elements_by_dimension ** order} and there are only {len(env[t][1])} available)')
    
      dims = [elements_by_dimension] * order
      format = ['pt.dense'] * order
      format_as_str = str(format).translate({39 : None})
      t_declarations += f'pt.tensor({dims}, fmt = pt.format({format_as_str}), dtype = pt.int32, name = \'{t}\')\n'

    # Initialize non-scalar tensors.
    values = env[t][1]
    values_idx = 0
    coords = [[*(range(elements_by_dimension))] for _ in range(order)]
    for coord in itertools.product(*coords):
      t_initializations += f'{t}.insert({list(coord)}, {values[values_idx]})\n'
      values_idx += 1

    defined[t] = True

  # Write computation and evaluate the left-hand side.
  index_vars_definition = 'i, j, k, l = pt.get_index_vars(4)\n'
  computation = candidate.lhs.replace('(', '[').replace(')', ']') + ' = '
  computation += re.sub(r'\(([i-l|,]+)\)', r'[\1]', candidate.rhs) + '\n'
  for t in tensors:
    if candidate.get_order(t) == 0:
      computation = computation.replace(f'{t}', f'{t}[None] ', 1)

  computation += 'a.evaluate()\n'

  # Convert to a NumPy flatten array.
  conversion = 'flatten_a = a.to_array().flatten()\n'
  # Print out results.
  # Set NumPy print options so the array is not truncated when printed.
  print_results = 'np.set_printoptions(threshold=np.inf)\n'
  print_results += 'print(flatten_a)\n'

  pytaco_program = imports + t_declarations + t_initializations + index_vars_definition + computation + conversion + print_results
  return pytaco_program
  

def check_as_pytaco(candidate, io, binding):
  """Check if a candidate is the correct solution by implementing and 
  interpreting a PyTaco program given a substituion.
  """
  try:
    env = build_env(candidate.get_lhs(), candidate.get_order(candidate.get_lhs()), binding, io)
    pytaco_program = write_pytaco_program(candidate, env)

  except InsufficientElements as ie:
    raise RuntimeError('Invalid binding' + ': ' + str(ie))

  # Get output from Python dynamically executed code
  # https://stackoverflow.com/a/3906390
  f = StringIO()
  with redirect_stdout(f):
    exec(pytaco_program)

  taco_output = [int(value) for value in re.split('\[|\]|\n| ', f.getvalue()) if value.lstrip('-').isnumeric()]
  return taco_output 


def check_binding(binding, c, io_set, debug = False):
  """Check if a candidate is the solution using a specific substution."""
  if debug:
    print(f'Checking binding: {binding}')
  try:
    # We first check agains the first sample in the IO set
    taco_output = check_as_pytaco(c, io_set[0], binding)
    if debug:
      print(f'TACO output: {taco_output[:10]}')
      print(f'Expected output: {io_set[0].output[1][:10]}')
    if taco_output == io_set[0].output[1]:
      # A candidate is correct if it returns the correct output for all
      # the elements in the IO set.
      for io in io_set[1:]:
        taco_output = check_as_pytaco(c, io, binding)
        if debug:
          print(f'TACO output: {taco_output[:10]}')
          print(f'Expected output: {io_set[0].output[1][:10]}')
        if taco_output != io.output[1]:
          return False
        
      return True
    else:
      return False
  except RuntimeError as e:
    raise e
  

def check(candidate, io_set, debug = False):
  """Check if a candidate is the solution for the synthesis problem."""
  # We can discard candidates based only in the shape of the IO.
  # Since all IO samples have the same shape, we need to check only one item 
  # from the IO set.
  if not is_io_compatible(candidate, io_set[0]):
    print(f'Ruling out {candidate}')
    return CheckingReturnCode.TYPE_DISCARDED
  
  print(f'Running {candidate}')
  input_bindings = get_bindings_permutation(candidate, io_set[0])
  n_runtime_errors = 0
  # We check a candidate with all the possible substitions. We stop
  # as soon as we find the first substitution that leads to the 
  # correct answer.
  for binding in input_bindings:
    try:
      if check_binding(binding, candidate, io_set, debug):
        return CheckingReturnCode.SUCCESS
    except RuntimeError:
      n_runtime_errors += 1
      continue
  
  # If there was an runtime error for all the possible bindings for this candidate
  # we classifiy it as RUNTIME_ERROR, otherwise at there was at least one valid
  # binding, but still gives us the wrong output.
  if n_runtime_errors == len(input_bindings):
    return CheckingReturnCode.RUNTIME_ERROR
  else:
    return CheckingReturnCode.CANDIDATE_TRIED