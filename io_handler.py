from clang.cindex import Index, CursorKind
import itertools
import json
import os
from typing import Dict, List, NamedTuple

LOOP_TYPES = [CursorKind.FOR_STMT, CursorKind.WHILE_STMT, CursorKind.DO_STMT]

class Variable(NamedTuple):
  """ A Variable object holds its dimension (1 for scalars and the length in case of arrays)."""
  dimension: int
  values: List[int]


class IOExample(NamedTuple):
  """ An IOExample object contains an input dictionary mapping names to Variables,
 a dictionary mapping constants to its values. The key in this case is a symbolic
 name. Finally, this object keeps the output variable.
 """
  input: Dict[str, Variable]
  constants: Dict[str, int]
  output: Variable


def parse_program(program_path):
  """Parse a C program and return the corresponding translation unit."""
  idx = Index.create()
  tu = idx.parse(program_path, args = ['-c'])
  return tu


def extract_clang(cursor):
  """ Extract the text from the source code that corresponds to the AST
  cursor.
  """
  if cursor is None:
    return ''
  filename = cursor.location.file.name
  with open(filename, 'r') as fh:
    contents = fh.read()
  return contents[cursor.extent.start.offset: cursor.extent.end.offset]


def get_nodes_by_kind(tu, kinds):
  """ Return a list of nodes with the given kinds."""
  return [n for n in tu.cursor.walk_preorder() if n.kind in kinds]


def get_loop_control_vars(tu):
  """ Get the variables that are used as control variables in all the loops
  found in source code.
  """
  loops = get_nodes_by_kind(tu, LOOP_TYPES)
  loop_control_vars = set()
  for l in loops:
    # We assume that the loops is the classical form, therefore, the loop condition
    # is the second element in the list formed by the children of the loop node in the AST.
    # Variables are saved using their hash to avoid duplicates.
    loop_cond = list(l.get_children())[1]
    if loop_cond.kind == CursorKind.BINARY_OPERATOR:
      loop_control_vars.add(list(loop_cond.get_children())[0].get_definition().hash)

  return loop_control_vars


def get_assignments(tu):
  """Return all the assignment statements found in source code, including compound assignments."""
  binop_exprs = get_nodes_by_kind(tu, [CursorKind.BINARY_OPERATOR])
  compound_assigments = get_nodes_by_kind(tu, [CursorKind.COMPOUND_ASSIGNMENT_OPERATOR])
  return compound_assigments +  [binop for binop in binop_exprs for tok in binop.get_tokens() if '=' == tok.spelling]


def get_constants(program_path):
  """Return the relevant constants found in source code. We consider relevants constants that appear
  on the right-hand side of assignments and that are not used in loop initialization expressions.
  """
  tu = parse_program(program_path)
  assignments = get_assignments(tu)
  cons = set()
  visited = set()
  loop_vars = get_loop_control_vars(tu)
  for a in assignments:
    lhs = list(a.get_children())[0]
    if lhs.kind == CursorKind.DECL_REF_EXPR:
      # We do not consider constants used to initialize loop variables.
      if lhs.get_definition().hash in loop_vars:
        continue
    
    # We keep track of the visit constant nodes to avoid duplicates.
    for c in list(a.get_children())[1].walk_preorder():
      # We are only interested in constants that appear on the RHS of assignments.
      if c.kind == CursorKind.UNARY_OPERATOR:
        unary_operand = list(c.get_children())[0]
        if unary_operand.kind == CursorKind.INTEGER_LITERAL and unary_operand.hash not in visited:
          cons.add(int(extract_clang(c)))
          visited.add(unary_operand.hash)

      elif c.kind == CursorKind.INTEGER_LITERAL and c.hash not in visited:
        cons.add(int(extract_clang(c)))

      visited.add(c.hash)

  return cons


class IOHandler():
  """IOHandler is a class responsible to read an IO file in the JSON format containing 
  the different IO samples and convert it into a set of IOExample objects.
  """
  @staticmethod
  def parse_io(program_path, io_path):
    io_set = []
    try:
      with open(io_path, 'r') as io_file:
        io_pairs = json.load(io_file)
    except FileNotFoundError as e:
      raise e
    
    # The IO files do not hold information regarding constants in the original program. We analyze the
    # source code to retrieve relevant constants. In case no constants are found, the Constants field
    # in the IOExample object will be an empty dictionary.
    io_constants = dict()
    constants = get_constants(program_path)
    if constants:
      constant_id = itertools.count(1)
      for c in constants:
        io_constants[f'Cons{next(constant_id)}'] = c
       
    for sample in io_pairs:
      io_input_vars = dict()

      output_values = list(sample['output'].values())[0]
      io_output_var = Variable(int(output_values[0]), [int(value) for value in output_values[1].split()])

      for in_var, in_values in sample['input'].items():
        io_input_vars[in_var] = Variable(int(in_values[0]), [int(val) for val in in_values[1].split()])

      io_set.append(IOExample(io_input_vars, io_constants, io_output_var))
      
    return io_set