import flask.testing
import git
from git.objects.fun import (tree_entries_from_data as git_objects_fun_tree_entries_from_data)
from gitdb.db.loose import (LooseObjectDB as gitdb_db_loose_LooseObjectDB)
from json import (loads as json_loads)
from os import (makedirs as os_makedirs)
from os.path import (dirname as os_path_dirname,
                     exists as os_path_exists,
                     isdir as os_path_isdir)
import pytest
from server import (server_app,
                    SERVER_REPO_DIR,
                    SERVER_SERVER_NAME,
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

def req_get(
    client: flask.testing.FlaskClient,
    path: str,
    data
):
    rv = client.get(base_url="http://api.localhost.localdomain:5000", path=path, data=data)
    assert rv.status_code == 200
    return rv

@pytest.fixture(scope="session")
def repodir_s(tmpdir_factory):
    return tmpdir_factory.mktemp("repo_s")

@pytest.fixture(scope="session")
def monkeypatch_server_repo_dir(repodir_s):
    '''monkeypatch server configuration (SERVER_REPO_DIR)'''
    import server
    server.SERVER_REPO_DIR = repodir_s

@pytest.fixture(scope="session")
def rc(tmpdir_factory) -> ServerRepoCtx:
    repodir = tmpdir_factory.mktemp("repo")
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
        with _file_open_mkdirp(str(rc.repodir.join(ff[0]))) as f:
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
    server_app.config['TESTING'] = True
    c = server_app.test_client()

    with server_app.app_context():
        pass

    yield c

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
    rv = req_get(client, "/refs/heads/master", "")
    r: dict = json_loads(rv.data)
    assert type(r["tree"]) == str
    return r["tree"]

def _get_object(
    client: flask.testing.FlaskClient,
    obj: shahex
) -> bytes:
    rv = req_get(client, "/object/" + obj, "")
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

def test_monkeypatch_must_be_first(monkeypatch_server_repo_dir):
    pass

def test_fixtures_ensure(
    rc_s: ServerRepoCtx,
    rc: ServerRepoCtx
):
    pass

def test_get_root(
    client: flask.testing.FlaskClient
):
    req_get(client, "/sub/", "hello")

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
    assert os_path_exists(str(rc.repodir.join(".git")))
    rc.repo.head.reset(master.commit, index=True, working_tree=True)
