from timestamp import get_latest_str as timestamp__get_latest_str
import flask
from base64 import b32encode as base__b32encode
from coor import execself as coor__execself
from json import loads as json__loads
from flask import Flask as flask__Flask
from flask import request as flask__request
from os import urandom as os__urandom
from os import environ as os__environ
from subprocess import run as subprocess__run
from urllib.parse import urljoin as urllib_parse__urljoin

confdict = dict

server_app: flask.Flask = flask__Flask(__name__, static_url_path = "")

def ps_url_for(u):
    if 'X-Real-ROOT' in flask__request.headers:
        return urllib_parse__urljoin(flask__request.headers['X-Real-ROOT'], u)
    else:
        raise RuntimeError()

@server_app.route("/build", methods=["GET"])
def build():
    stagedir: str = server_app.config['PS']['STAGEDIR']
    deploy: str = server_app.config['PS']['DEPLOYSCRIPT']
    subprocess__run(deploy, shell=True, timeout=300, check=True)
    ts: str = timestamp__get_latest_str(stagedir)
    return f'''<p>Timestamp: <b>{ts}</b></p>'''

@server_app.route("/commit", methods=["GET"])
def commit():
    stagedir: str = server_app.config['PS']['STAGEDIR']
    repodir: str = server_app.config['PS']['REPODIR']
    coor__execself('refs/heads/master', stagedir, repodir)
    return f'''okay'''

@server_app.route("/", methods=["GET"])
def index():
    stagedir: str = server_app.config['PS']['STAGEDIR']
    ts: str = timestamp__get_latest_str(stagedir)
    return f'''
<p>Timestamp: <b>{ts}</b></p>
<p><a href="{ps_url_for('build')}">Build</a></p>
<p><a href="{ps_url_for('commit')}">Commit</a></p>
'''

def server_config_flask_and_run(_config: confdict):
    global server_app
    server_app.secret_key = base__b32encode(os__urandom(24)).decode("UTF-8")
    server_app.config['PS'] = _config
    server_app.config['SERVER_NAME'] = _config['ORIGIN_DOMAIN_APP'] + ':' + _config['LISTEN_PORT']
    server_app.config['TESTING'] = _config['TESTING']
    server_app.run(host = server_app.config['PS']['LISTEN_HOST'], port = server_app.config['PS']['LISTEN_PORT'])

def server_run_overridable():
    try: conf = json__loads(os__environ['PS_CONFIG'])
    except: import ps_config_deploy; conf = ps_config_deploy.config.copy()
    server_config_flask_and_run(conf)
    server_run()

server_run_overridable()
