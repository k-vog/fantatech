#!/usr/bin/env python3

import glob
import os
import shutil
import subprocess
import sys

def run(args, quiet=True):
  cmd = ['./bin/ftconv'] + args
  r = subprocess.run(cmd, capture_output=quiet, text=True)
  if r.returncode != 0:
    cmd_str = ' '.join(cmd)
    print(f'[torture] ERROR! Command "{cmd_str}" returned {r.returncode}')
    if quiet:
      print(f'[torture] stderr: {r.stderr}')
    # exit(1)

if __name__ == '__main__':
  os.chdir(os.path.join(os.path.dirname(__file__), '..'))

  gamedir = sys.argv[1]

  shutil.rmtree('./tmp/torture', ignore_errors=True)
  os.makedirs('./tmp/torture/torture1')
  os.makedirs('./tmp/torture/torture2')

  n = 0
  for f in glob.iglob(f'{gamedir}/*.lb5'):
    run([f, '--raw', './tmp/torture/torture1'], quiet=False)
    n += 1
  for f in glob.iglob(f'{gamedir}/*.bin'):
    run([f, '--raw', './tmp/torture/torture1'], quiet=False)
    n += 1

  print('Testing txt conversion')
  for f in glob.glob('./tmp/torture/torture1/*.txt'):
    d = f'./tmp/torture/torture2/{os.path.basename(f)}'
    run([f, d])
    n += 1

  print('Testing bmp conversion')
  for f in glob.glob('./tmp/torture/torture1/*.BMP'):
    d = os.path.splitext(os.path.basename(f))[0]
    d = f'./tmp/torture/torture2/{d}.bmp'
    run([f, d])
    n += 1

  print(f'Tested {n} files :)')
