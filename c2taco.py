import argparse
import os
import subprocess
import time

from synthesis.synthesizer import synthesize

def run_code_analysis(source_program, clang, analysis):
  if analysis == 'ProgramLength':
    analysis_dir = 'program_length'    
    plugin = analysis_dir.replace('_', '-')
  elif analysis == 'TensorOrders':
    analysis_dir = 'tensor_orders'
    plugin = analysis_dir.replace('_', '-')
  elif analysis == 'OperatorAnalysis':
    analysis_dir = 'operators'
    plugin = 'operator-analysis'
  else:
    print('Unkown analysis: ', analysis)
    exit(1)

  try:
   arguments = [clang, '-c', '-Xclang', '-load', '-Xclang', f'code_analysis/{analysis_dir}/build/lib{analysis}.so', '-Xclang', '-plugin', '-Xclang', plugin, source_program]
   command = subprocess.run(arguments, check = True, capture_output = True)
   output = command.stdout.rstrip().decode('utf-8')
   if analysis == 'ProgramLength':
     return int(output)
   elif analysis == 'TensorOrders':
     return [int(order) for order in output.split()]
   elif analysis == 'OperatorAnalysis':
     return output.split()
   
  except subprocess.CalledProcessError as e:
    print('Error while running plugin: ', e)
    # The length of the program is mandatory for the synthesizer, therefore, in case
    # the Program Length analysis fail, we return a default value of 0.
    return 0 if analysis == 'ProgramLength' else []


def c2taco():
  parser = argparse.ArgumentParser()
  parser.add_argument('program', type = str, help = 'Path to the program to be lifted')
  parser.add_argument('io', type = str, help = 'Path to input-output samples')
  parser.add_argument('clang', type = str, help = 'Path to Clang compiler')
  parser.add_argument('--no_log', action="store_false", required = False, help = 'Tell C2TACO to not produce synthesis logs.')
  args = parser.parse_args()
  assert os.path.isfile(args.program), f'Synthesizer could not find {args.program}'
  assert os.path.isfile(args.io), f'Synthesizer could not find {args.io}'
  assert os.path.isfile(args.clang), f'Synthesizer could not find {args.clang}'
  t_lift_start = time.time()
  length = run_code_analysis(args.program, args.clang, 'ProgramLength')
  orders = run_code_analysis(args.program, args.clang, 'TensorOrders')
  binops = run_code_analysis(args.program, args.clang, 'OperatorAnalysis')
  
  print(7 * '-', ' Synthesis arguments ', 7 * '-', '\n')
  print('Original program: ', args.program)
  print('IO: ', args.io)
  print('Expected length: ', length + 1) 
  if orders:
    print('Orders: ')
    for i in range(length + 1):
      print(f'  Tensor {i + 1}: {orders[i]}')
  
  print('Binary operators: ', binops, '\n', 7 * '-')
  synthesize(args.program, args.io, length, orders, binops, args.no_log)
  print(f'Total lifting time: {(time.time() - t_lift_start):.2f} seconds.\n\n')


if __name__ == '__main__':
  c2taco()