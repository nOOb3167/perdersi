import argparse
from os import (makedirs as os_makedirs)
from os.path import (dirname as os_path_dirname)
import pathlib
from pathlib import (Path as pathlib_Path)
import subprocess
from subprocess import (run as subprocess_run)

def err():
    raise RuntimeError();

def _file_open_mkdirp(path: str):
    os_makedirs(os_path_dirname(path), exist_ok=True)
    return open(path, "wb")

def deploy():
    from ps_config_deploy import config
    
    rsync_cmd = [
        config["RSYNC_CMD"],
        config["RSYNC_SRC_UHP"][0]]
    
    p0: subprocess.CompletedProcess = subprocess_run([str(rsync_exe)], capture_output=True)
    
    print(p0)

def run():
    # get args
    parser = argparse.ArgumentParser()
    args = parser.parse_args()
    #
    deploy()

if __name__ == '__main__':
    run()
