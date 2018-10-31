import argparse
import git
from os import (makedirs as os_makedirs)
from os.path import (dirname as os_path_dirname)
import pathlib
from pathlib import (Path as pathlib_Path)
from server import (ServerRepoCtx)
from shutil import (copyfile as shutil_copyfile)

def err():
    raise RuntimeError();

def _file_open_mkdirp(path: str):
    os_makedirs(os_path_dirname(path), exist_ok=True)
    return open(path, "wb")

def preload_repo(
    tupdater3_exe: str,
    stage2_exe: str,
    repodir_s: pathlib.Path
) -> ServerRepoCtx:
    with git.Repo.init(str(repodir_s)) as repo:
        rc = ServerRepoCtx(repodir_s, repo)
        # BEG populate with dummy data
        for ff in [("a.txt", b"aaa"), ("d/b.txt", b"bbb")]:
            with _file_open_mkdirp(str(rc.repodir.joinpath(ff[0]))) as f:
                f.write(ff[1])
            rc.repo.index.add([ff[0]])
        # END populate with dummy data
        # BEG populate with executables
        for gg in [("updater.exe", tupdater3_exe), ("stage2.exe", stage2_exe)]:
            shutil_copyfile(gg[1], str(rc.repodir.joinpath(gg[0])))
            rc.repo.index.add([gg[0]])
        # END populate with executables
        commit = rc.repo.index.commit("ccc")
        ref_master = git.Reference(rc.repo, "refs/heads/master")
        ref_master.set_object(commit)

def run():
    # get args
    parser = argparse.ArgumentParser()
    parser.add_argument('--updater_exe', nargs=1, required=True, type=lambda x: x if x.endswith(".exe") else err())
    parser.add_argument('--stage2_exe', nargs=1, required=True, type=lambda x: x if x.endswith(".exe") else err())
    parser.add_argument('--repo_dir', nargs=1, required=True)
    args = parser.parse_args()
    #
    preload_repo(args.updater_exe[0], args.stage2_exe[0], pathlib_Path(args.repo_dir[0]))

if __name__ == '__main__':
    run()
