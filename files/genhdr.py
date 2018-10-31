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

def make_hdr(json: str):
    byt: bytes = json.encode("UTF-8")
    hex: str = byt.hex()
    return 'char g_ps_config[] = { ' + ''.join(['0x' + hex[2*i] + hex[2*i+1] + ", " for i in range(len(hex) // 2)]) + '};\n'

def run():
    # get args
    parser = argparse.ArgumentParser()
    parser.add_argument('--src_mod', nargs=1, required=True)
    parser.add_argument('--dst', nargs=1, required=True, type=lambda x: x if x.endswith(".h") else err())
    args = parser.parse_args()
    # import src_mod and attempt to retrieve its config attribute as json
    src_mod: types.ModuleType = importlib.import_module(str(args.src_mod[0]))
    src_config: dict = src_mod.config
    src_config_json: str = json.dumps(src_config)
    # write dst
    dst: pathlib.Path = pathlib.Path(args.dst[0])
    with dst.open(mode='w', newline='\n') as f:
        f.write(make_hdr(src_config_json))

if __name__ == '__main__':
    run()
