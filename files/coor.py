import git
import git.cmd
import pathlib
import typing
from argparse import ArgumentParser as argparse__ArgumentParser
from git import Repo as git__Repo
from glob import glob as glob__glob
from pathlib import Path as pathlib__Path
from shutil import copy2 as shutil__copy2
from shutil import copytree as shutil__copytree
from shutil import rmtree as shutil__rmtree
from subprocess import run as subprocess__run
from sys import executable as sys__executable

# https://github.com/gitpython-developers/GitPython/issues/292
#   repo.git.add(A=True) vs repo.index.add(???)

def repo_create_ensure(repodir: str):
    try:
        return git__Repo(repodir)
    except (git.exc.InvalidGitRepositoryError, git.exc.NoSuchPathError):
        return git__Repo.init(repodir)

def copy_from_todir(src: pathlib.Path, dstdir: pathlib.Path):
    assert src.name != '.git'
    assert dstdir.is_dir()
    dst: pathlib.Path = dstdir.joinpath(src.name)
    if dst.exists():
        if dst.is_dir():
            shutil__rmtree(dst)
        else:
            dst.unlink()
    if src.is_dir():
        shutil__copytree(src, dst)
    else:
        shutil__copy2(src, dst)

def copy_fromdir_todir(srcdir: pathlib.Path, dstdir: pathlib.Path):
    for x in [pathlib__Path(a) for a in glob__glob(str(srcdir.joinpath('*')))]:
        copy_from_todir(src=x, dstdir=dstdir)

def run():
    # get args
    parser = argparse__ArgumentParser()
    parser.add_argument('--refname', nargs=1, required=True)
    parser.add_argument('--stagedir', nargs=1, required=True)
    parser.add_argument('--repodir', nargs=1, required=True)
    args = parser.parse_args()
    refname: str = args.refname[0]
    stagedir: pathlib.Path = pathlib__Path(args.stagedir[0])
    repodir: pathlib.Path = pathlib__Path(args.repodir[0])
    assert stagedir.is_dir() and repodir.is_dir()
    repo: git.Repo = repo_create_ensure(str(repodir))
    # repo clean
    repo.git.clean(d=True, f=True)
    repo.git.rm('.', r=True)
    # stage copy to repo
    copy_fromdir_todir(srcdir=stagedir, dstdir=repodir)
    # repo add files and commit
    repo.git.add(A=True)
    commit: git.Commit = repo.index.commit('ccc')
    # repo set ref
    ref = git.Reference(repo, refname)
    ref.set_object(commit)

def execself(refname: str, stagedir: str, repodir: str):
    modulename: str = __name__
    subprocess__run([sys__executable, '-m', modulename, '--refname', refname, '--stagedir', stagedir, '--repodir', repodir], timeout=300, check=True)

if __name__ == '__main__':
    run()
