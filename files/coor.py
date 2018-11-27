import git
import pathlib
from argparse import ArgumentParser as argparse__ArgumentParser
from git import Repo as git__Repo
from pathlib import Path as pathlib__Path
from sys import executable as sys__executable
from subprocess import run as subprocess__run

# https://github.com/gitpython-developers/GitPython/issues/292
#   repo.git.add(A=True) vs repo.index.add(???)

def repo_create_ensure(repodir: str):
    try:
        return git__Repo(repodir)
    except (git.exc.InvalidGitRepositoryError, git.exc.NoSuchPathError):
        return git__Repo.init(repodir)

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
    repo.git.add(A=True)
    commit: git.Commit = repo.index.commit('ccc')
    ref = git.Reference(repo, refname)
    ref.set_object(commit)

def execself(refname: str, stagedir: str, repodir: str):
    modulename: str = __name__
    subprocess__run([sys__executable, '-m', modulename, '--refname', refname, '--stagedir', stagedir, '--repodir', repodir], timeout=300, check=True)

if __name__ == '__main__':
    run()
