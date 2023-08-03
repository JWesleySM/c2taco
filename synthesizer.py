import exrex
import re
import os
import time

from candidate import Candidate
from io_handler import IOHandler
from check import check, get_tensor_order, CheckingReturnCode

# The list of binary operators supported in TACO. It is used in Enumerative Template Synthesis (ETS).
BINOPS = ['+', '-', '*', '/']
constant_id = 1

def write_log_io_fail(program):
  """Write the synthesis log in case of not finding an IO file."""
  with open(os.path.join(os.path.dirname(program), f'{os.path.basename(program)[:-2]}-lifting.log'), 'w') as log_file:
    log_file.write(f'Log for {program}\n')
    log_file.write('Solution: no IO file found\n')


def write_log(program, solution, io_time, enumerate_time, checking_time, tried, type_discarded, error_discarded, total_candidates, ETS = False):
  """Write the log of the synthesis process."""
  with open(os.path.join(os.path.dirname(program), f'{os.path.basename(program)[:-2]}-lifting.log'), 'w') as log_file:
    log_file.write(f'Log for {program}\n')
    if solution:
      log_file.write(f'Solution: {solution.lhs} = {solution.rhs}\n')
    else:
      log_file.write(f'Solution not found\n')
    log_file.write(f'Search space size: {total_candidates} candidates\n')
    log_file.write(f'Candidates tried: {tried}\n')
    log_file.write(f'Candidates discarded by type checking: {type_discarded}\n')
    log_file.write(f'Candidates error: {error_discarded}\n')
    log_file.write(f'Time for parsing IO: {io_time:.2f} seconds\n')
    log_file.write(f'Time for enumerate candidates: {enumerate_time:.2f} seconds\n')
    log_file.write(f'Time for checking candidates: {checking_time:.2f} seconds\n')
    log_file.write(f'Total synthesis time: {(io_time + enumerate_time + checking_time):.2f} seconds\n')
    if ETS:
      log_file.write(f'Simple Enumerative Template Snthesis was used\n')
    

def break_naming_sequence(prog, t):
  """Check if a tensor can be inserted in an program. A tensor
  can be inserted only if it does not break the naming sequence
  (b->c->d->e).
  """
  for i in range(ord('b'), ord(t)):
    if chr(i) not in prog:
      return True

  return False


def redefine_tensor(prog, t):
  """Check if the tensor t is already present in the TACO program, the
  new reference does not mutate its order.
  For example, if we have the program a = b(i) and the synthesizer tries
  to add another reference to 'b', such a reference must have order 1, for
  is the order of 'b(i)' already in the program.
  """
  for elem in prog.split():
    if(re.match(f'^-?[{t[0]}]', elem)):
      if get_tensor_order(elem) != get_tensor_order(t):
        return True

  return False


def is_non_supported_program(lhs, prog, op, t):
  """Check if insert tensor 't' in the current program does not
  yield an invalid TACO program. More specifically, TACO does
  only supports a scalar in the left-hand side of a program if
     - all the tensors in the right-hand side are also scalars or
     - the binary operator is multiplication.
  For example, a = b(i) * c(i) is valid, but a = b(i) + c(i) is
  invalid.
  IMPORTANT: this function returns true if the resulting program is
  invalid.
  """
  if op == '*':
    return False

  if get_tensor_order(lhs) > 0 or get_tensor_order(t) == 0:
    return False
  elif any(get_tensor_order(t) > 0 for t in prog.split()):
    return True

  return False


def is_ilegal_insertion(lhs, prog, op, t):
  """Check if adding a reference to a tensor 't' to program 'prog' produces an invalid
  program. Adding a new reference 't'in a program is illegal if
    1 - 't' changes its own order in case of a previous reference to the same tensor
    2 - adding 't' breaks the naming sequence
    3 - adding 't' produces a program currently not supported by TACO.
  """
  return redefine_tensor(prog, t) or break_naming_sequence(prog, t[0]) or is_non_supported_program(lhs, prog, op, t)


def is_valid_indexation(t):
  """Check if a same index variable is indexing two different
  dimensions of a tensor, which is illegal in TACO. For example,
  the reference 'a(i,i)' is not valid for the variable 'i' is used
  to index both dimensions of 'a'.
  """
  if get_tensor_order(t) < 2:
    return True

  return not any(t.count(index_var) > 1 for index_var in ['i','j','k','l']) 



def get_valid_indexations(tensor, order):
  """Return a list with all the possible manners to reference a tensor
  of a certain order.
  """
  valid_indexations = f'{tensor}'
  if order > 0:
    valid_indexations += '\('
    for _ in range(order):
      valid_indexations += '(i|j|k),'
    valid_indexations = valid_indexations.rstrip(',') + '\)'
  
  return [t for t in exrex.generate(valid_indexations) if is_valid_indexation(t)]


def get_search_space(size, orders, binops, include_constants):
  """Return the search space restricted by the argument features. In other words,
  this function return all possible programs with 'size' tensors, each one with its
  order defined in the 'orders' list, combining the binary operators in 'binops'. The
  'include_constants' argument tells the synthesizer whether ir needs to consider 
  programs that contain constants.
  """
  global constant_id
  candidates = []
  if size == 1:
    # <EXPR> ::= CONSTANT
    if include_constants:
      candidates += [Candidate(lhs, f'Cons{constant_id}') for lhs in get_valid_indexations('a', orders[0])]
      constant_id += 1
    # <EXPR> ::= <TENSOR> | -<TENSOR>
    candidates += [Candidate(lhs, rhs) for lhs in get_valid_indexations('a', orders[0])
                   for rhs in get_valid_indexations('-?b', orders[1])]
    return candidates

  # The programs are built in a bottom-up fashion
  sub_candidates = get_search_space(size - 1, orders, binops, include_constants)
  for c in sub_candidates:
    # <EXPR> ::= <EXPR> + <EXPR> | <EXPR> - <EXPR> | <EXPR> * <EXPR> | <EXPR> / <EXPR>
    for op in binops:
      if include_constants:
        candidates.append(Candidate(c.lhs, c.rhs + ' ' + op + f' Cons{constant_id}'))
        constant_id += 1

      for i in range(ord('b'), ord('b') + size):
        t = chr(i)
        # A tensor reference can only ne added if:
        #   1 - it does not break naming sequence
        #   2 - if it is a repeated reference in the program, it has the same order than the previous reference.
        
        candidates += [Candidate(c.lhs, c.rhs + ' ' + op + ' ' + tensor) for tensor in get_valid_indexations(t, orders[size])
                       if not is_ilegal_insertion(c.lhs, c.rhs, op, tensor)]
  return candidates


def get_grammar_regex(t):
  """Returns a regular expression that depicts all the possibilities of references to
  a tensor up to order 4. Used by ETS.
  """
  return f'({t}|{t}\((i|j|k)\)|{t}\((i|j|k),(i|j|k)\)|{t}\((i|j|k),(i|j|k),(i|j|k)\)|{t}\((i|j|k),(i|j|k),(i|j|k)\),(i|j|k)\))'


def ETS(size):
  """This function implements enumerative synthesis of templates (ETS). It builds a list of 
  all the possible TACO programs with a given size (number of tensors/constants).
  """
  global constant_id
  candidates = []
  if size == 1:
    # <EXPR> ::= <CONSTANT>
    candidates += [Candidate(lhs, f'Cons{constant_id}') for lhs in exrex.generate(get_grammar_regex('a'))
                   if is_valid_indexation(lhs)]
    constant_id += 1
    # <EXPR> ::= <TENSOR> | -<TENSOR>
    candidates += [Candidate(lhs, rhs) for lhs in exrex.generate(get_grammar_regex('a'))
                   for rhs in exrex.generate('-?' + get_grammar_regex('b'))
                   if is_valid_indexation(lhs) and is_valid_indexation(rhs)]
    
    return candidates

  # The programs are built in a bottom-up fashion
  sub_candidates = ETS(size - 1)
  for c in sub_candidates:
    # <EXPR> ::= <EXPR> + <EXPR> | <EXPR> - <EXPR> | <EXPR> * <EXPR> | <EXPR> / <EXPR>
    for op in BINOPS:
      candidates.append(Candidate(c.lhs, c.rhs + ' ' + op + f' Cons{constant_id}'))
      constant_id += 1

      for i in range(ord('b'), ord('b') + size):
        t = chr(i)
        # A tensor can only ne added if:
        #   1 - it does not break naming sequence
        #   2 - if it is a repeated reference in the program, it has the same order than the previous appearance
        
        candidates += [Candidate(c.lhs, c.rhs + ' ' + op + ' ' + tensor) for tensor in exrex.generate(get_grammar_regex(t))
                       if is_valid_indexation(tensor) and not is_ilegal_insertion(c.lhs, c.rhs, op, tensor)]

  return candidates


def check_candidates(candidates, io_set):
  """Checks a list of candidates agains a set of IO examples looking
  for a solution to the synthesis problem.
  """
  # Statistics for synthesis log.
  t_checking_start = time.time()
  type_discarded = 0
  error_discarded = 0
  tried = 0
  for c in candidates:
    # Check a candidate and analyse the return code to correctly increment
    # statistics.
    check_return_code = check(c, io_set)
    if check_return_code == CheckingReturnCode.TYPE_DISCARDED:
      type_discarded += 1
    elif check_return_code == CheckingReturnCode.RUNTIME_ERROR:
      error_discarded += 1
    elif check_return_code == CheckingReturnCode.CANDIDATE_TRIED:
      tried += 1

    # A valid solution is found.
    if check_return_code == CheckingReturnCode.SUCCESS:
      tried += 1
      checking_time = time.time() - t_checking_start
      print(f'{c} is the answer\n')
      return c, type_discarded, error_discarded, tried, checking_time
    
  # All the candidates were inspected but none of them is correct.
  checking_time = time.time() - t_checking_start
  return None, type_discarded, error_discarded, tried, checking_time


def synthesize(original, size, orders, binops):
  """Solve the synthesis problem of findiing a TACO program equivalent to the
  original implementation. We initially enumerate candiddates in the search 
  space driven by program features.
  """
  original_path = os.path.abspath(original)
  # Create a set of IO samples
  t_io_start = time.time()
  try:
    io_set = IOHandler.parse_io(original_path)
  except FileNotFoundError:
    write_log_io_fail(original_path)
    exit(1)

  io_time = time.time() - t_io_start

  # We check whether there are constant values in the io samples
  # In positive case, we should consider candidates that contains
  # constants. We can exclude them from search space otherwise.
  include_constants = True if io_set[0].constants else False
  t_enumerate_start = time.time()
  candidates = get_search_space(size, orders, binops, include_constants)
  enumerate_time = time.time() - t_enumerate_start
  total_candidates = len(candidates)
  c, type_discarded, error_discarded, tried, checking_time = check_candidates(candidates, io_set)

  if c:
    write_log(original_path, c, io_time, enumerate_time, checking_time, tried, type_discarded, error_discarded, total_candidates)
  else:
    # If we explore the entire search space built by the initial features and still
    # did not find the correct solution, we check all possibilities using simple
    # enumerative template synthesis.
    t_enumerate_start = time.time()
    ETS_candidates = ETS(size)
    enumerate_time = time.time() - t_enumerate_start
    total_candidates = len(ETS_candidates)
    c, type_discarded, error_discarded, tried, checking_time = check_candidates(ETS_candidates, io_set)
    if c:
      write_log(original_path, c, io_time, enumerate_time, checking_time, tried, type_discarded, error_discarded, total_candidates, ETS = True)
      return
    
    # No solution was found
    write_log(original_path, None, io_time, enumerate_time, checking_time, tried, type_discarded, error_discarded, total_candidates, ETS = True)
  