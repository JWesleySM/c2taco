import exrex
import re
import os
import time

from candidate import Candidate
from io_handler import IOHandler
from check import check, get_tensor_order, CheckingReturnCode

BINOPS = ['+', '-', '*', '/']
constant_id = 1

def write_log_io_fail(program):
  with open(os.path.join(os.path.dirname(program), f'{os.path.basename(program)[:-2]}-lifting.log'), 'w') as log_file:
    log_file.write(f'Log for {program}\n')
    log_file.write('Solution: no IO file found\n')


def write_log(program, solution, io_time, enumerate_time, checking_time, tried, io_discarded, error_discarded, total_candidates, ETS = False):
  with open(os.path.join(os.path.dirname(program), f'{os.path.basename(program)[:-2]}-lifting.log'), 'w') as log_file:
    log_file.write(f'Log for {program}\n')
    if solution:
      log_file.write(f'Solution: {solution.lhs} = {solution.exp}\n')
    else:
      log_file.write(f'Solution not found\n')
    log_file.write(f'Search space size: {total_candidates} candidates\n')
    log_file.write(f'Candidates tried: {tried}\n')
    log_file.write(f'Candidates discarded by type checking: {io_discarded}\n')
    log_file.write(f'Candidates error: {error_discarded}\n')
    log_file.write(f'Time for parsing IO: {io_time:.2f} seconds\n')
    log_file.write(f'Time for enumerate candidates: {enumerate_time:.2f} seconds\n')
    log_file.write(f'Time for checking candidates: {checking_time:.2f} seconds\n')
    log_file.write(f'Total synthesis time: {(io_time + enumerate_time + checking_time):.2f} seconds\n')
    if ETS:
      log_file.write(f'Simple Enumerative Template Snthesis was used\n')
    

# Check if a tensor can be inserted in an expression. A tensor
# can be inserted only if it does not break the naming sequence
# (b->c->d->e)
def break_naming_sequence(exp, t):
  for i in range(ord('b'), ord(t)):
    if chr(i) not in exp:
      return True

  return False


def redefine_tensor(exp, t):
  for elem in exp.split():
    if(re.match(f'^-?[{t[0]}]', elem)):
      if get_tensor_order(elem) != get_tensor_order(t):
        return True

  return False


def is_non_supported_expression(lhs, exp, op, t):
  if op == '*':
    return False

  if get_tensor_order(lhs) > 0 or get_tensor_order(t) == 0:
    return False
  elif any(get_tensor_order(t) > 0 for t in exp.split()):
    return True

  return False


def is_ilegal_insertion(lhs, exp, op, t):
  return redefine_tensor(exp, t) or break_naming_sequence(exp, t[0]) or is_non_supported_expression(lhs, exp, op, t)


# check if a same variable is indexing two dimensions
# of a tensor
def is_valid_indexation(t):
  if get_tensor_order(t) < 2:
    return True

  return not any(t.count(index_var) > 1 for index_var in ['i','j','k','l']) 


def get_valid_indexations(tensor, order):
  valid_indexations = f'{tensor}'
  if order > 0:
    valid_indexations += '\('
    for _ in range(order):
      valid_indexations += '(i|j|k),'
    valid_indexations = valid_indexations.rstrip(',') + '\)'
  
  return [t for t in exrex.generate(valid_indexations) if is_valid_indexation(t)]


def get_search_space(size, orders, binops, include_constants):
  global constant_id
  candidates = []
  if size == 1:
    # E -> C
    if include_constants:
      candidates += [Candidate(lhs, f'Cons{constant_id}') for lhs in get_valid_indexations('a', orders[0])]
      constant_id += 1
    # E -> t | -t
    candidates += [Candidate(lhs, rhs) for lhs in get_valid_indexations('a', orders[0])
                   for rhs in get_valid_indexations('-?b', orders[1])]
            
    return candidates

  sub_candidates = get_search_space(size - 1, orders, binops, include_constants)
  for c in sub_candidates:
    # E -> E binop E
    for op in binops:
      if include_constants:
        candidates.append(Candidate(c.lhs, c.exp + ' ' + op + f' Cons{constant_id}'))
        constant_id += 1

      for i in range(ord('b'), ord('b') + size):
        t = chr(i)
        # A tensor can only ne added if:
        #   1 - it does not break naming sequence
        #   2 - if it is being reinserted in the expression, it has the same order than the previous appearance
        
        candidates += [Candidate(c.lhs, c.exp + ' ' + op + ' ' + tensor) for tensor in get_valid_indexations(t, orders[size])
                       if not is_ilegal_insertion(c.lhs, c.exp, op, tensor)]
  return candidates


def get_grammar_regex(t):
  return f'({t}|{t}\((i|j|k)\)|{t}\((i|j|k),(i|j|k)\)|{t}\((i|j|k),(i|j|k),(i|j|k)\))'


def ETS(size):
  global constant_id
  candidates = []
  if size == 1:
    # E -> C
    candidates += [Candidate(lhs, f'Cons{constant_id}') for lhs in exrex.generate(get_grammar_regex('a'))
                   if is_valid_indexation(lhs)]
    constant_id += 1
    # E -> t | -t
    candidates += [Candidate(lhs, rhs) for lhs in exrex.generate(get_grammar_regex('a'))
                   for rhs in exrex.generate('-?' + get_grammar_regex('b'))
                   if is_valid_indexation(lhs) and is_valid_indexation(rhs)]
    
    return candidates

  sub_candidates = ETS(size - 1)
  for c in sub_candidates:
    # E -> E binop E
    for op in BINOPS:
      candidates.append(Candidate(c.lhs, c.exp + ' ' + op + f' Cons{constant_id}'))
      constant_id += 1

      for i in range(ord('b'), ord('b') + size):
        t = chr(i)
        # A tensor can only ne added if:
        #   1 - it does not break naming sequence
        #   2 - if it is being re-inserted in the expression, it has the same order than the previous appearance
        
        candidates += [Candidate(c.lhs, c.exp + ' ' + op + ' ' + tensor) for tensor in exrex.generate(get_grammar_regex(t))
                       if is_valid_indexation(tensor) and not is_ilegal_insertion(c.lhs, c.exp, op, tensor)]

  return candidates


def check_candidates(candidates, io_set):
  t_checking_start = time.time()
  io_discarded = 0
  error_discarded = 0
  tried = 0
  for c in candidates:
    check_return_code = check(c, io_set)
    if check_return_code == CheckingReturnCode.IO_DISCARDED:
      io_discarded += 1
    elif check_return_code == CheckingReturnCode.RUNTIME_ERROR:
      error_discarded += 1
    elif check_return_code == CheckingReturnCode.CANDIDATE_TRIED:
      tried += 1

    if check_return_code == CheckingReturnCode.SUCCESS:
      tried += 1
      checking_time = time.time() - t_checking_start
      print(f'{c} is the answer\n')
      return c, io_discarded, error_discarded, tried, checking_time
    
  checking_time = time.time() - t_checking_start
  return None, io_discarded, error_discarded, tried, checking_time


def synthesize(original, size, orders, binops):
  original_path = os.path.abspath(original)
  # Create IO set
  t_io_start = time.time()
  try:
    io_set = IOHandler.parse_io(original_path)
  except FileNotFoundError:
    write_log_io_fail(original_path)
    exit(1)

  io_time = time.time() - t_io_start

  # We check whether there are constant values in the io samples
  # In positive case, we should consider candidates that contains
  # constants. We can exclude them from search space otherwise
  include_constants = True if io_set[0].constants else False
  t_enumerate_start = time.time()
  candidates = get_search_space(size, orders, binops, include_constants)
  enumerate_time = time.time() - t_enumerate_start
  total_candidates = len(candidates)
  c, io_discarded, error_discarded, tried, checking_time = check_candidates(candidates, io_set)

  if c:
    write_log(original_path, c, io_time, enumerate_time, checking_time, tried, io_discarded, error_discarded, total_candidates)
  else:
    # If we explore the entire search space built by the initial features and still
    # did not find the correct solution, we check all possibilities using simple
    # enumerative template synthesis
    t_enumerate_start = time.time()
    ETS_candidates = ETS(size)
    enumerate_time = time.time() - t_enumerate_start
    total_candidates = len(ETS_candidates)
    c, io_discarded, error_discarded, tried, checking_time = check_candidates(ETS_candidates, io_set)
    if c:
      write_log(original_path, c, io_time, enumerate_time, checking_time, tried, io_discarded, error_discarded, total_candidates, ETS = True)
      return
    
    write_log(original_path, None, io_time, enumerate_time, checking_time, tried, io_discarded, error_discarded, total_candidates, ETS = True)
  