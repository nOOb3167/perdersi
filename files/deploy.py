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
import typing

def sub_cygpath(path: str):
    # FIXME: this is a windows / cygwin workaround
    #   subs 'X:/path' into '/cygdrive/x/path'
    def drivelower(m: re.Match):
        return '/cygdrive/' + m[1].lower() + '/' + m[2]
    return re_sub(r'^([a-zA-Z]):[/\\](.*)', drivelower, path)

def run():
    from ps_config_deploy import config
    
    # RSYNC_CMD: "rsync -x -y zzz"
    #   split into "rsync" "-x" "-y" "zzz"
    # RSYNC_DST_UHP: ["user", "host", "port"]
    #   user@host:port
    #   vanishing atsign on empty user: user@XXX XXX
    #   remaining colon on empty port:  XXX:port XXX:
    # RSYNC_DST: "/path/"
    # RSYNC_SRC: "c:\\path"
    #   windows paths get cygwin-ed
    
    cmd: typing.List[str] = shlex_split(config['RSYNC_CMD'])
    
    user = config['RSYNC_DST_UHP'][0]
    host = config['RSYNC_DST_UHP'][1]
    port = config['RSYNC_DST_UHP'][2]
    
    src = sub_cygpath(config['RSYNC_SRC'])
    dst = (user + ('@' if user else '') + host + ':' + port) + config['RSYNC_DST']
    
    p0: subprocess.CompletedProcess = subprocess_run(cmd + ['-rav', src, dst], shell=True, capture_output=True)
    if p0.returncode != 0:
        raise RuntimeError()

if __name__ == '__main__':
    run()
