import flask.testing
import git
from git.objects.fun import (tree_entries_from_data as git_objects_fun_tree_entries_from_data)
from gitdb.db.loose import (LooseObjectDB as gitdb_db_loose_LooseObjectDB)
from json import (loads as json_loads)
from os import (makedirs as os_makedirs)
from os.path import (dirname as os_path_dirname,
                     exists as os_path_exists,
                     isdir as os_path_isdir)
import pathlib
from pathlib import (Path as pathlib_Path)
import pytest
from re import (fullmatch as re_fullmatch)
import server
from server import (config as server_config,
                    server_app,
                    server_config_make_default,
                    server_run_prepare,
                    ServerRepoCtx)
from typing import Callable, List, Set, Tuple
from zlib import (decompress as zlib_decompress)

# FileMode are values of PS_GIT_FILEMODE_???
FileMode = int
shahex = str
shabin = bytes

PS_GIT_FILEMODE_TREE            = 0o0040000
PS_GIT_FILEMODE_BLOB            = 0o0100644
PS_GIT_FILEMODE_BLOB_EXECUTABLE = 0o0100755

class RetCodeErr(Exception):
    pass
class RetCode500(RetCodeErr):
    pass

def _testing_make_server_config(repodir: pathlib.Path):
    _config = server_config_make_default()
    _config["REPO_DIR"] = str(repodir)
    _config["TESTING"] = True
    return _config

def _testing_make_server_config_env(repodir: pathlib.Path):
    import json, os
    env = os.environ.copy()
    env["PS_CONFIG"] = json.dumps(_testing_make_server_config(repodir))
    return env

@pytest.fixture(scope="session")
def repodir(tmpdir_factory) -> pathlib.Path:
    return pathlib_Path(tmpdir_factory.mktemp("repo"))

@pytest.fixture(scope="session")
def repodir_updater(tmpdir_factory) -> pathlib.Path:
    return pathlib_Path(tmpdir_factory.mktemp("repo_updater"))

@pytest.fixture(scope="session")
def repodir_s(tmpdir_factory) -> pathlib.Path:
    return pathlib_Path(tmpdir_factory.mktemp("repo_s"))

@pytest.fixture(scope="session")
def server_run_prepare_for_testing(repodir_s):
    server_run_prepare(_testing_make_server_config(repodir_s))

@pytest.fixture(scope="session")
def rc(repodir) -> ServerRepoCtx:
    repodir = repodir
    repo = git.Repo.init(repodir)
    yield ServerRepoCtx(repodir, repo)

@pytest.fixture(scope="session")
def rc_s(repodir_s) -> ServerRepoCtx:
    repo = git.Repo.init(str(repodir_s))
    rc = ServerRepoCtx(repodir_s, repo)
    # BEG populate with dummy data
    ffs = [("a.txt", b"aaa"),
           ("d/b.txt", b"bbb")]
    for ff in ffs:
        with _file_open_mkdirp(str(rc.repodir.joinpath(ff[0]))) as f:
            f.write(ff[1])
    for ff in ffs:
        rc.repo.index.add([ff[0]])
    commit = rc.repo.index.commit("ccc")
    ref_master = git.Reference(rc.repo, "refs/heads/master")
    ref_master.set_object(commit)
    # END populate with dummy data
    yield rc

@pytest.fixture
def client() -> flask.testing.FlaskClient:
    yield server_app.test_client()

def _force_origin_presence(kwargs):
    if "headers" not in kwargs:
        kwargs["headers"] = {}
    if "Origin" not in kwargs["headers"]:
        kwargs["headers"]["Origin"] = "http://localhost.localdomain:5201"

def _req_post(
    client: flask.testing.FlaskClient,
    path: str,
    data,
    **kwargs
):
    _force_origin_presence(kwargs)
    rv = client.post(base_url="http://api.localhost.localdomain:5201", path=path, data=data, **kwargs)
    if rv.status_code == 500:
        raise RetCode500()
    if rv.status_code != 200:
        raise RetCodeErr()
    return rv

def _file_open_mkdirp(path: str):
    os_makedirs(os_path_dirname(path), exist_ok=True)
    return open(path, "wb")

def _bin2hex(bbb: shabin):
    return bbb.hex()
def _hex2bin(sss: shahex):
    return bytes.fromhex(sss)

def _get_master_tree_hex(
    client: flask.testing.FlaskClient
) -> shahex:
    rv = _req_post(client, "/refs/heads/master", "")
    assert re_fullmatch("[0-9a-fA-F]{40}", rv.data.decode("UTF-8"))
    return rv.data.decode("UTF-8")

def _get_object(
    client: flask.testing.FlaskClient,
    obj: shahex
) -> bytes:
    rv = _req_post(client, "/objects/" + obj[:2] + "/" + obj[2:], "")
    objloose: bytes = rv.data
    return objloose

def _get_trees(
    client: flask.testing.FlaskClient,
    loosedb,
    tree: shahex
) -> List[shahex]:
    ''' get and write trees '''
    _loosedb_raw_object_write(loosedb, tree, _get_object(client, tree))
    out: List[shahex] = [tree]
    t: shahex
    for t in _tree_stream_entry_filter(loosedb.stream(_hex2bin(tree)), lambda filemode: filemode == PS_GIT_FILEMODE_TREE):
        out.extend(_get_trees(client, loosedb, t))
    return out

def _get_blobs(
    client: flask.testing.FlaskClient,
    loosedb,
    blobs: List[shahex]
) -> List[shahex]:
    ''' write blobs '''
    b: shahex
    for b in blobs:
        _loosedb_raw_object_write(loosedb, b, _get_object(client, b))

def _list_tree_blobs(
    loosedb,
    trees: List[shahex]
) -> List[shahex]:
    ''' get blobs recursively traversable from tree '''
    blobs: List[shahex] = []
    tree: shahex
    for tree in trees:
        blobs.extend(_tree_stream_entry_filter(
            loosedb.stream(_hex2bin(tree)),
            lambda filemode: \
                filemode == PS_GIT_FILEMODE_BLOB or \
                filemode == PS_GIT_FILEMODE_BLOB_EXECUTABLE))
    return blobs

def _tree_stream_entry_filter(
    treestream,
    predicate: Callable[[FileMode], bool]
):
    entries: List[Tuple[shabin, FileMode, str]] = git_objects_fun_tree_entries_from_data(treestream.read())
    return [_bin2hex(x[0]) for x in entries if predicate(x[1])]

def _loosedb_raw_object_write(loosedb, presumedhex: shahex, objloose: bytes):
    # assert not loosedb.has_object(_hex2bin(presumedhex))
    objpath = loosedb.db_path(loosedb.object_path(presumedhex))
    # assert not os_path_exists(objpath)
    os_makedirs(os_path_dirname(objpath), exist_ok=True)
    with _file_open_mkdirp(objpath) as f:
        f.write(objloose)
    # FIXME:
    #loosedb.update_cache(force=True)
    assert loosedb.has_object(_hex2bin(presumedhex))

def test_monkeypatch_must_be_first(server_run_prepare_for_testing):
    pass

def test_fixtures_ensure(
    rc_s: ServerRepoCtx,
    rc: ServerRepoCtx
):
    pass

def test_sub_post(
    client: flask.testing.FlaskClient
):
    _req_post(client, "/sub/", "hello")

def test_sub_post_csrf(
    client: flask.testing.FlaskClient
):
    with pytest.raises(server.CsrfExc):
        _req_post(client, "/sub/", "hello", headers={"Origin": "http://bad.localhost.localdomain:5201"})
    with pytest.raises(server.CsrfExc):
        _req_post(client, "/sub/", "hello", headers={"Origin": "http://localdomain:5201"})
    with pytest.raises(server.CsrfExc):
        _req_post(client, "/sub/", "hello", headers={"Origin": "http://localhost.localdomain:1111"})
    with pytest.raises(server.CsrfExc):
        _req_post(client, "/sub/", "hello", headers={"Origin": "http://localhost.localdomain"})

def test_get_head(
    client: flask.testing.FlaskClient
):
    _get_master_tree_hex(client)

def test_get_head_object(
    client: flask.testing.FlaskClient
):
    master_tree: shahex = _get_master_tree_hex(client)
    master_tree_loose = _get_object(client, master_tree)
    treedata: bytes = zlib_decompress(master_tree_loose)
    assert treedata.find(b"tree") != -1 and treedata.find(b"a.txt") != -1

def test_get_head_trees(
    rc: ServerRepoCtx,
    client: flask.testing.FlaskClient
):
    master_tree: shahex = _get_master_tree_hex(client)
    loosedb: gitdb_db_loose_LooseObjectDB = rc.repo.odb
    trees: List[shahex] = _get_trees(client, loosedb, master_tree)
    assert len(trees) == 2
    tree: shahex
    for tree in trees:
        assert loosedb.has_object(_hex2bin(tree))

def test_get_head_blobs(
    rc: ServerRepoCtx,
    client: flask.testing.FlaskClient
):
    master_tree: shahex = _get_master_tree_hex(client)
    trees: List[shahex] = _get_trees(client, rc.repo.odb, master_tree)
    blobs: List[shahex] = _list_tree_blobs(rc.repo.odb, trees)
    assert len(blobs) == 2
    bwritten = _get_blobs(client, rc.repo.odb, blobs)
    datas: Set[binary] = { b"aaa", b"bbb" }
    for b in blobs:
        assert rc.repo.odb.stream(_hex2bin(b)).read() in datas

def test_commit_head(
    rc: ServerRepoCtx,
    client: flask.testing.FlaskClient
):
    master_tree: shahex = _get_master_tree_hex(client)
    dummy: git.Actor = git.Actor('dummy', "dummy@dummy.dummy")
    commit: git.Commit = git.Commit.create_from_tree(rc.repo, master_tree, message="", author=dummy, committer=dummy)
    master: git.Reference = git.Reference(rc.repo, "refs/heads/master")
    master.set_commit(commit)

def test_checkout_head(
    rc: ServerRepoCtx
):
    master: git.Reference = git.Reference(rc.repo, "refs/heads/master")
    # FIXME: only touches paths recorded in index - stray worktree files not removed etc
    #   see IndexFile.checkout vs head.checkout
    # FIXME: use force=True ?
    #index: git.IndexFile = git.IndexFile.from_tree(rc.repo, master.commit.tree)
    #index.checkout()
    assert os_path_exists(str(rc.repodir.joinpath(".git")))
    rc.repo.head.reset(master.commit, index=True, working_tree=True)

def test_updater(
    customopt_debug_wait: str,
    customopt_python_exe: str,
    customopt_updater_exe: str,
    repodir_updater: pathlib.Path,
    repodir_s: pathlib.Path,
    client: flask.testing.FlaskClient
):
    import subprocess

    p0 = subprocess.Popen([customopt_updater_exe], env=_testing_make_server_config_env(repodir=repodir_updater))
    p1 = subprocess.Popen([customopt_python_exe, "server.py"], env=_testing_make_server_config_env(repodir=repodir_s))
    try: p0.communicate(timeout=(None if customopt_debug_wait == "ON" else 10))
    except: pass
    try: p0.kill()
    except: pass
    try: p1.kill()
    except: pass
    
    if p1.returncode != 0:
        raise RetCodeErr()
