#!/usr/bin/env python3

from datetime import datetime as datetime__datetime
from os import walk as os__walk
from os.path import getmtime as os_path__getmtime
from pathlib import Path as pathlib__Path
from typing import List
from typing import Tuple

dirname_t = str
fname_t = str
pathname_t = str
stat_t = dict

def getmtime(p: pathname_t) -> float:
    try:
        return os_path__getmtime(p)
    except FileNotFoundError:
        return 0.0

def get_latest(p: dirname_t) -> float:
    walk: List[Tuple[dirname_t, List[dirname_t], List[fname_t]]] = list(os__walk(p))

    dirs: List[dirname_t] = [w[0] for w in walk]
    files: List[fname_t] = [pathlib__Path(w[0]) / str(f) for w in walk for f in w[2]]

    dirs_max = max([getmtime(v) for v in dirs])
    files_max = max([getmtime(v) for v in files])

    any_max = max(dirs_max, files_max)
    return any_max

def get_latest_str(p: dirname_t) -> str:
    any_max = get_latest(p)
    any_max_s = datetime__datetime.utcfromtimestamp(any_max).strftime('%Y-%m-%d %H:%M:%S UTC')
    return any_max_s
