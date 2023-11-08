import itertools
import re
from synthesis.check import get_tensor_order

class Candidate():
  """This class is used to represent a candidate TACO program during synthesis.
  Each candidate contains a left-hand and right-hand side, a map of tensors 
  to their respective orders and a unique identifier.
  """
  id = itertools.count()
  tensors_regex = '^-?[b-e]|Cons'
  constant_tensor_regex = 'Cons*.'
  
  def __init__(self, lhs, rhs):
    """Create a candidate."""
    self.id = next(Candidate.id)
    self.lhs = lhs
    self.rhs = rhs
    self.tensor_orders = dict()
    self.tensor_orders[self.__get_tensor_name__(lhs)] = get_tensor_order(lhs)
    for elem in rhs.split():
      if re.match(Candidate.tensors_regex, elem):
        self.tensor_orders[self.__get_tensor_name__(elem)] = get_tensor_order(elem)


  def __repr__(self):
    """Represent a Candidate object as a string."""
    return f'{self.lhs} = {self.rhs}'

 
  def get_n_tensors(self):
    """Return the number of tensors in the Candidate."""
    return 1 + sum(1 for elem in self.rhs.split() if re.match(Candidate.tensors_regex, elem))


  def get_tensor_orders(self):  
    """Return a list with the order of the tensors in the Candidate from left to right.
    The head of the list is the order of the tensor in the left-hand side of the program.
    """  
    return list(self.tensor_orders.values())


  def get_order(self, tensor):
    """Return the order of a specific tensor in the Candidate."""
    return self.tensor_orders[tensor]


  def get_lhs(self):
    """Return the tensor on the left-hand side of the Candidate."""
    return self.lhs[0]


  def __get_tensor_name__(self, tensor):
    """Return the name of a tensor give its representation in the program.
    For example, given a tensor 't(k,j,l)', this method returns 't'.
    Tensor names are single characters.
    """
    if re.match(Candidate.constant_tensor_regex, tensor):
      return tensor
    else:
      # In case of unary negation, the name of the tensor is in the second position
      # of its string.
      return tensor[1] if tensor[0] == '-' else tensor[0]


  def get_tensors(self):
    """Return a list with all the tensor in the Candidate."""
    return [self.__get_tensor_name__(self.lhs)] + [self.__get_tensor_name__(elem) for elem in self.rhs.split() if re.match(Candidate.tensors_regex, elem)]
  

  def has_constant(self):
    """Return true if the Candidate contains constants in its program and false otherwise."""
    return True if any(re.match(Candidate.constant_tensor_regex, tensor) for tensor in self.get_tensors()) else False


