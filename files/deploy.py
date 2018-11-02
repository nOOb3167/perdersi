import argparse
from os import (makedirs as os_makedirs)
from os.path import (dirname as os_path_dirname)
import pathlib
from pathlib import (Path as pathlib_Path)
import re
from re import (sub as re_sub)
from shlex import (split as shlex_split)
import subprocess
from subprocess import (run as subprocess_run)
from sys import (stderr as sys_stderr)

def err():
    raise RuntimeError();

def _file_open_mkdirp(path: str):
    os_makedirs(os_path_dirname(path), exist_ok=True)
    return open(path, 'wb')

def deploy():
    from ps_config_deploy import config
    
    user = config['RSYNC_DST_UHP'][0]
    host = config['RSYNC_DST_UHP'][1]
    port = config['RSYNC_DST_UHP'][2]
    
    cmd = shlex_split(config['RSYNC_CMD'])
    
    # FIXME: this is a windows / cygwin workaround
    #   needs to sub 'X:/path' into '/cygdrive/x/path'
    def drivelower(m: re.Match):
        return '/cygdrive/' + m[1].lower() + '/' + m[2]
    config['RSYNC_SRC'] = re_sub(r'^([a-zA-Z]):[/\\](.*)', drivelower, config['RSYNC_SRC'])
    
    uhp = user + ('@' if user else '') + host + ':' + port
    dst = uhp + config['RSYNC_DST']
    
    xcmd = cmd
    xcmd += ['-rav', config['RSYNC_SRC'], dst]
    
    try:
        p0: subprocess.CompletedProcess = subprocess_run(xcmd, shell=True, capture_output=True)
        print(p0)
    except:
        print(f'failure running: {cmd}', file=sys_stderr)
        raise

def run():
    deploy()

if __name__ == '__main__':
    run()
