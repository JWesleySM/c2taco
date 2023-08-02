import itertools
import re
from check import get_tensor_order

class Candidate():
  id = itertools.count()
  tensors_regex = '^-?[b-e]|Cons'
  constant_tensor_regex = 'Cons*.'
  
  def __init__(self, lhs, exp):
    self.id = next(Candidate.id)
    self.lhs = lhs
    self.exp = exp
    self.tensor_orders = dict()
    self.tensor_orders[self.__get_tensor_name__(lhs)] = get_tensor_order(lhs)
    for elem in exp.split():
      if re.match(Candidate.tensors_regex, elem):
        self.tensor_orders[self.__get_tensor_name__(elem)] = get_tensor_order(elem)


  def __repr__(self):
    return f'{self.lhs} = {self.exp}'

 
  def get_n_tensors(self):
    return 1 + sum(1 for elem in self.exp.split() if re.match(Candidate.tensors_regex, elem))


  def get_tensor_orders(self):    
    return list(self.tensor_orders.values())


  def get_order(self, tensor):
    return self.tensor_orders[tensor]


  def get_lhs(self):
    return self.lhs[0]


  def __get_tensor_name__(self, tensor):
    if re.match(Candidate.constant_tensor_regex, tensor):
      return tensor
    else:
      return tensor[1] if tensor[0] == '-' else tensor[0]


  def get_tensors(self):
    return [self.__get_tensor_name__(self.lhs)] + [self.__get_tensor_name__(elem) for elem in self.exp.split() if re.match(Candidate.tensors_regex, elem)]
  

  def has_constant(self):
    return True if any(re.match(Candidate.constant_tensor_regex, tensor) for tensor in self.get_tensors()) else False


