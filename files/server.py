from config import config, almost_random
from flask import (current_app as flask_current_app,
                   Flask as flask_Flask,
                   g as flask_g,
                   jsonify as flask_jsonify,
                   request as flask_request,
                   session as flask_session)
import git
import os
import urllib.parse
from werkzeug.wsgi import (wrap_file as werkzeug_wsgi_wrap_file)

# http://flask.pocoo.org/docs/1.0/testing/
# http://flask.pocoo.org/docs/1.0/appcontext/#storing-data
# http://flask.pocoo.org/docs/1.0/api/#flask.Flask.teardown_appcontext
# https://github.com/pytest-dev/pytest/issues/2508  # fixture finalizers
# https://stackoverflow.com/questions/6624453/whats-the-correct-way-to-convert-bytes-to-a-hex-string-in-python-3/36149089#36149089
# https://stackoverflow.com/questions/13317536/get-a-list-of-all-routes-defined-in-the-app/13318415#13318415
# https://www.python.org/dev/peps/pep-0366/  # relative import __package__ hack

SERVER_SERVER_NAME = config["ORIGIN_DOMAIN_APP"] + ":" + config["LISTEN_PORT"]
SERVER_SESSION_KEY = 'perdersi_session'

server_app = flask_Flask(__name__, static_url_path = "")
server_app.secret_key = almost_random()
server_app.config["SERVER_NAME"] = SERVER_SERVER_NAME

class CsrfExc(Exception):
        pass

def server_check_csrf():
        if "Origin" not in flask_request.headers:
            raise CsrfExc()
        u1 = "http://" + config["ORIGIN_DOMAIN_APP"] + ":" + config["LISTEN_PORT"]
        u2 = flask_request.headers['Origin']
        if u1 != u2:
            raise CsrfExc()

def server_compare_urlparse_csrf(u1, u2):
        return (u1.scheme == u2.scheme) and (u1.netloc == u2.netloc)

class ServerRepoCtx:
    def __init__(self, repodir, repo):
        self.repodir = repodir
        self.repo = repo
    @classmethod
    def create_ensure(cls, repodir):
        try:
            repo = git.Repo(repodir)
        except git.exc.InvalidRepositoryError:
            repo = git.Repo.init(repodir)
        return ServerRepoCtx(repodir, repo)

def server_repo_ctx_get():
    ''' within Application Context '''
    if "rc" not in flask_g:
        flask_g.rc = ServerRepoCtx.create_ensure(config["REPO_DIR"])
    return flask_g.rc
@server_app.teardown_appcontext
def server_repo_ctx_teardown(err):
    pass

def server_route_api_get(path):
    return server_app.route(path, subdomain="api", methods=["GET"])
def server_route_api_post(path):
    return server_app.route(path, subdomain="api", methods=["POST"])

@server_route_api_get("/refs/heads/<refname_p>")
def refs_heads(refname_p):
    refname = "refs/heads/" + refname_p
    rc = server_repo_ctx_get()
    ref = git.Reference(rc.repo, refname)
    commit = ref.commit
    tree = commit.tree
    return flask_jsonify({ "tree": tree.hexsha })

@server_route_api_get("/object/<objhex>")
def object(objhex):
    objbin = bytes.fromhex(objhex)
    rc = server_repo_ctx_get()
    obj = git.Object(rc.repo, objbin)
    objectsdir = rc.repodir.join(".git/objects/")
    objectpath = objectsdir.join(objhex[:2]).join("/").join(objhex[2:])
    objectfile = open(str(objectpath), mode="rb")
    fileiter = werkzeug_wsgi_wrap_file(flask_request.environ, objectfile)
    return flask_current_app.response_class(fileiter, content_type="application/octet-stream", direct_passthrough=True)

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
