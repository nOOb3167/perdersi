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
from zlib import (decompress as zlib_decompress)

PS_GIT_FILEMODE_TREE            = 0o0040000
PS_GIT_FILEMODE_BLOB            = 0o0100644
PS_GIT_FILEMODE_BLOB_EXECUTABLE = 0o0100755

def req_get(client, path, data):
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
def repoctx(tmpdir_factory):
    repodir = tmpdir_factory.mktemp("repo")
    repo = git.Repo.init(repodir)
    yield ServerRepoCtx(repodir, repo)

@pytest.fixture(scope="session")
def repoctx_s(repodir_s):
    repo = git.Repo.init(str(repodir_s))
    rc = ServerRepoCtx(repodir_s, repo)
    try:
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
    except:
        raise
    yield rc

@pytest.fixture
def client():
    server_app.config['TESTING'] = True
    c = server_app.test_client()

    with server_app.app_context():
        pass

    yield c

def _file_open_mkdirp(pathstr):
    os_makedirs(os_path_dirname(pathstr), exist_ok=True)
    return open(pathstr, "wb")

def _bin2hex(bbb):
    return bbb.hex()
def _hex2bin(sss):
    return bytes.fromhex(sss)

def _get_master_tree_hex(client):
    rv = req_get(client, "/refs/heads/master", "")
    r = json_loads(rv.data)
    return r["tree"]

def _get_object(client, objhex):
    rv = req_get(client, "/object/" + objhex, "")
    objloose = rv.data
    return objloose

def _get_trees(repoctx, client, tree_hex):
    # FIXME: just use repoctx.repo.odb
    loosedir = str(_repodir_get_loosedir(repoctx.repodir))
    loosedb = gitdb_db_loose_LooseObjectDB(loosedir)
    treehexlist = _tree_recurse_0(client, repoctx, loosedb, tree_hex)
    return treehexlist

def _loosedb_raw_object_write(loosedb, presumedhex, objloose):
    assert not loosedb.has_object(_hex2bin(presumedhex))
    objpath = loosedb.db_path(loosedb.object_path(presumedhex))
    assert not os_path_exists(objpath)
    os_makedirs(os_path_dirname(objpath), exist_ok=True)
    with _file_open_mkdirp(objpath) as f:
        f.write(objloose)
    # FIXME:
    #loosedb.update_cache(force=True)
    assert loosedb.has_object(_hex2bin(presumedhex))

def _repodir_get_loosedir(repodir):
    loosedir = repodir.join(".git/objects")
    assert os_path_isdir(str(loosedir))
    return loosedir

def _tree_recurse_0(client, repo, loosedb, tree_hex):
    treeloose = _get_object(client, tree_hex)
    _loosedb_raw_object_write(loosedb, tree_hex, treeloose)
    stream = loosedb.stream(_hex2bin(tree_hex))
    stream_data = stream.read()
    # entries: [(binsha : bytes, git filemode : int, filename : str), ...]
    entries = git_objects_fun_tree_entries_from_data(stream_data)
    treehexs = [_bin2hex(x[0]) for x in entries if x[1] == PS_GIT_FILEMODE_TREE]
    out = [tree_hex]
    for x in treehexs:
        out.extend(_tree_recurse_0(client, repo, loosedb, x))
    return out

def test_monkeypatch_must_be_first(monkeypatch_server_repo_dir):
    pass

def test_get_root(repoctx_s, client):
    req_get(client, "/sub/", "hello")

def test_get_head(client):
    _get_master_tree_hex(client)

def test_get_head_object(repoctx_s, client):
    master_tree_hex = _get_master_tree_hex(client)
    master_tree_loose = _get_object(client, master_tree_hex)
    treedata = zlib_decompress(master_tree_loose)
    assert treedata.find(b"tree") != -1 and treedata.find(b"a.txt") != -1

def test_get_head_trees(repoctx_s, repoctx, client):
    master_tree_hex = _get_master_tree_hex(client)
    treehexlist = _get_trees(repoctx, client, master_tree_hex)
    assert len(treehexlist) == 2
    
