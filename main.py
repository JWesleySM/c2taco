import argparse
import os
import time
from synthesizer import synthesize


def enumerative_guided_synthesis():
  print("hello")
  parser = argparse.ArgumentParser()
  parser.add_argument('program', type = str, help = 'Path to the program to be lifted')
  parser.add_argument('io', type = str, help = 'Path to input-output samples')
  parser.add_argument('length', type = int, help = 'Length of TACO expression to the synhtesizer initial guess')
  parser.add_argument('--orders', nargs = '*', type = int, help = 'Order of tensors expected in the solution')
  parser.add_argument('--binops', nargs = '*', type = str, help = 'Binary operators expected in the solution')
  parser.add_argument('--no_log', action="store_false", required = False, help = 'Tell C2TACO to not produce synthesis logs.')
  args = parser.parse_args()
  print('IO: ', args.io)
  assert os.path.isfile(args.program), f'Synthesizer could not find {args.program}'
  assert os.path.isfile(args.io), f'Synthesizer could not find {args.program}'
  t_synth_start = time.time()
  print(7 * '-', ' Synthesis arguments ', 7 * '-', '\n')
  print('Original program: ', args.program)
  print('IO: ', args.io)
  print('Expected length: ', args.length)
  if args.orders:
    print('Orders: ')
    for i in range(args.length):
      print(f'  Tensor {i + 1}: {args.orders[i]}')
  
  binops = [binop.replace("\'", '') for binop in args.binops] if args.binops else []
  print('Binary operators: ', binops, '\n', 7 * '-')
  synthesize(args.program, args.io, args.length, args.orders, binops, args.no_log)
  print(f'Total synthesis time: {(time.time() - t_synth_start):.2f} seconds.\n\n')


if __name__ == '__main__':
  enumerative_guided_synthesis()