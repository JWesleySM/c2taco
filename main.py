import argparse
import os
import time
from synthesizer import synthesize


def enumerative_smart_synthesis():
  parser = argparse.ArgumentParser()
  parser.add_argument('program', type = str, help = 'Path to the program to be lifted')
  parser.add_argument('length', type = int, help = 'Length of TACO expression to the synhtesizer initial guess')
  parser.add_argument('--orders', nargs='*', type = int, help = 'Order of tensors expected in the solution')
  parser.add_argument('--binops', nargs='*', type = str, help = 'Binary operators expected in the solution')
  args = parser.parse_args()
  assert os.path.isfile(args.program), f'Synthesizer could not find {args.program}'
  t_synth_start = time.time()
  print(7 * '-', ' Synthesis arguments ', 7 * '-', '\n')
  print('Original program: ', args.program)
  print('Expected length: ', args.length)
  assert len(args.orders) == args.length + 1, 'Provide one order for each tensor on the expected solution'
  print('Orders: ')
  for i in range(args.length):
    print(f'  Tensor {i}: {args.orders[i]}')
  
  binops = [binop.replace("\'", '') for binop in args.binops] if args.binops else []
  print('Binary operators: ', binops, '\n', 7 * '-')
  synthesize(args.program, args.length, args.orders, binops)
  print(f'Total synthesis time: {(time.time() - t_synth_start):.2f} seconds\n\n')


if __name__ == '__main__':
  enumerative_smart_synthesis()