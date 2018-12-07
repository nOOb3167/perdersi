#!/usr/bin/python3

# invocation
#   python genhdr.py --src_mod config_updater_default --dst header.h
# file input
#   config_updater_default: config = { 'K': 'V' }
# file created
#   header.h: char g_ps_config[] = { ... };

import argparse
import importlib
import json
import pathlib
import sys
import types

def err():
    raise RuntimeError();

def make_hdr(nam: str, byt: bytes):
    hex: str = byt.hex()
    chex: str = ''.join(['0x' + hex[2*i] + hex[2*i+1] + ', ' for i in range(len(hex) // 2)])
    return f'char g_{nam}[] = {{ {chex} }};\n'

def mod(src_mod: str):
    # import src_mod and attempt to retrieve its config attribute as json
    src_mod: types.ModuleType = importlib.import_module(src_mod)
    src_config: dict = src_mod.config
    src_config_json: str = json.dumps(src_config)
    byt: bytes = src_config_json.encode("UTF-8")
    return byt

def pth(src_pth: str):
    src: pathlib.Path = pathlib.Path(src_pth)
    with src.open(mode='rb') as f:
        return f.read()

def run():
    # get args
    parser = argparse.ArgumentParser()
    parser.add_argument('--src_mod', nargs=1, required=False)
    parser.add_argument('--src_pth', nargs=1, required=False)
    parser.add_argument('--dst', nargs=1, required=True, type=lambda x: x if x.endswith(".h") else err())
    args = parser.parse_args()
    # either mod or pth
    byt = mod(str(args.src_mod[0])) if args.src_mod is not None else \
          pth(str(args.src_pth[0])) if args.src_pth is not None else \
          err()
    # write dst
    dst: pathlib.Path = pathlib.Path(args.dst[0])
    nam: str = dst.stem
    with dst.open(mode='w', newline='\n') as f:
        f.write(make_hdr(nam, byt))

if __name__ == '__main__':
    run()
