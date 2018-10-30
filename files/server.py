import base64
import flask
from flask import (current_app as flask_current_app,
                   Flask as flask_Flask,
                   g as flask_g,
                   jsonify as flask_jsonify,
                   request as flask_request,
                   session as flask_session)
import git
from os import (name as os_name,
                urandom as os_urandom)
import pathlib
from pathlib import (Path as pathlib_Path)
import urllib.parse
from werkzeug.wsgi import (wrap_file as werkzeug_wsgi_wrap_file)

confdict = dict

# http://flask.pocoo.org/docs/1.0/testing/
# http://flask.pocoo.org/docs/1.0/appcontext/#storing-data
# http://flask.pocoo.org/docs/1.0/api/#flask.Flask.teardown_appcontext
# https://github.com/pytest-dev/pytest/issues/2508  # fixture finalizers
# https://stackoverflow.com/questions/6624453/whats-the-correct-way-to-convert-bytes-to-a-hex-string-in-python-3/36149089#36149089
# https://stackoverflow.com/questions/13317536/get-a-list-of-all-routes-defined-in-the-app/13318415#13318415
# https://www.python.org/dev/peps/pep-0366/  # relative import __package__ hack

server_app: flask.Flask = flask_Flask(__name__, static_url_path = "")

SERVER_SESSION_KEY = 'perdersi_session'

class CsrfExc(Exception):
        pass

def server_check_csrf():
        if "Origin" not in flask_request.headers:
            raise CsrfExc()
        allowed = [
            'http://' + flask_current_app.config['PS']['ORIGIN_DOMAIN_API'] + ':' + flask_current_app.config['PS']['LISTEN_PORT'],
            'http://' + flask_current_app.config['PS']['ORIGIN_DOMAIN_APP'] + ':' + flask_current_app.config['PS']['LISTEN_PORT'],
        ]
        incoming = flask_request.headers['Origin'].split(" ")
        for i in incoming:
            if i in allowed:
                return
        raise CsrfExc()

def server_compare_urlparse_csrf(u1, u2):
        return (u1.scheme == u2.scheme) and (u1.netloc == u2.netloc)

class ServerRepoCtx:
    def __init__(self, repodir: str, repo: git.Repo):
        self.repodir = pathlib_Path(repodir)
        self.repo = repo
    @classmethod
    def create_ensure(cls, repodir: str):
        try:
            repo = git.Repo(repodir)
        except (git.exc.InvalidGitRepositoryError, git.exc.NoSuchPathError):
            repo = git.Repo.init(repodir)
        return ServerRepoCtx(repodir, repo)

def server_repo_ctx_get():
    ''' within Application Context '''
    if "rc" not in flask_g:
        flask_g.rc = ServerRepoCtx.create_ensure(flask_current_app.config['PS']["REPO_DIR"])
    return flask_g.rc
@server_app.teardown_appcontext
def server_repo_ctx_teardown(err):
    pass

def server_route_api_post(path):
    return server_app.route(path, subdomain="api", methods=["POST"])

@server_route_api_post("/refs/heads/<refname>")
def refs_heads(refname):
    ref = git.Reference(server_repo_ctx_get().repo, "refs/heads/" + refname)
    commit = ref.commit
    tree = commit.tree
    #return flask_jsonify({ "tree": tree.hexsha })
    return tree.hexsha

@server_route_api_post("/objects/<objhex_a>/<objhex_b>")
def object(objhex_a, objhex_b):
    repopath: pathlib.Path = server_repo_ctx_get().repodir
    objectpath: pathlib.Path = repopath / ".git" / "objects" / objhex_a / objhex_b
    return flask_current_app.response_class(
        werkzeug_wsgi_wrap_file(flask_request.environ, open(str(objectpath), mode="rb")),
        content_type="application/octet-stream",
        direct_passthrough=True)

@server_route_api_post("/sub/")
def qqq():
    server_check_csrf()
    return f'''
    hello world qqq
    '''

@server_app.route("/", methods=["GET"])
def index():
        if SERVER_SESSION_KEY not in flask_session:
                flask_session[SERVER_SESSION_KEY] = {}
        return f'''
        hello world
        '''

def server_config_flask(_config: confdict):
    global server_app
    server_app.secret_key = base64.b32encode(os_urandom(24)).decode("UTF-8")
    server_app.config['PS'] = _config
    server_app.config["SERVER_NAME"] = _config['ORIGIN_DOMAIN_APP'] + ':' + _config['LISTEN_PORT']
    server_app.config['TESTING'] = "TESTING" in _config and _config['TESTING']

def server_run():
    global server_app
    server_app.run(host = server_app.config['PS']['LISTEN_HOST'], port = server_app.config['PS']['LISTEN_PORT'])

def server_run_overridable():
    import json, os
    conf = {}
    try:
        conf = json.loads(os.environ['PS_CONFIG'])
    except:
        import ps_config_server
        conf = ps_config_server.config.copy()
    server_config_flask(conf)
    server_run()
