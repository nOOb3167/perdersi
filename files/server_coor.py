import flask
import pathlib
import subprocess
import typing
from timestamp import get_latest_str as timestamp__get_latest_str
from base64 import b32encode as base__b32encode
from coor import execself as coor__execself
from json import loads as json__loads
from flask import Flask as flask__Flask
from flask import render_template_string as flask__render_template_string
from flask import request as flask__request
from flask import send_from_directory as flask__send_from_directory
from os import urandom as os__urandom
from os import environ as os__environ
from os import makedirs as os__makedirs
from re import fullmatch as re__fullmatch
from pathlib import Path as pathlib__Path
from subprocess import run as subprocess__run
from urllib.parse import urljoin as urllib_parse__urljoin

server_app: flask.Flask = flask__Flask(__name__, static_url_path = "")

def ps_url_for(u):
    if 'X-Real-ROOT' in flask__request.headers:
        return urllib_parse__urljoin(flask__request.headers['X-Real-ROOT'], u)
    else:
        raise RuntimeError()

def write_next(dstdir: pathlib.Path, data: str):
    os__makedirs(str(dstdir), exist_ok=True)
    files: typing.List[pathlib.Path] = [x for x in dstdir.iterdir() if x.is_file()]
    nmbrs: typing.List[int] = [fnameint(x.name) for x in files]
    snext: str = str(max(nmbrs, default=0) + 1) + '.txt'
    pnext: pathlib.Path = dstdir / snext
    with pnext.open(mode='w') as f:
        f.write(data)

def fnameint(fn: str):
    return int(re__fullmatch(r'([0-9]+)\.txt', fn).group(1))

@server_app.route("/buildinfo", methods=["GET"])
def buildinfo():
    bifdir: pathlib.Path = pathlib__Path(server_app.config['PS']['BUILDINFODIR'])
    files: typing.List[pathlib.Path] = [x for x in bifdir.iterdir() if x.is_file()]
    fnams: typing.List[str] = sorted([x.name for x in files], key=fnameint, reverse=True)
    tvars: dict = { 'fnams': fnams, 'ps_url_for': ps_url_for }
    return flask__render_template_string('''
    <ul>
    {% for f in fnams %}
      <li><a href="{{ ps_url_for('buildinfo/' + f) }}">{{ f }}</a></li>
    {% endfor %}
    </ul>
    ''',
    **tvars)
@server_app.route("/buildinfo/<string:bi>")
def buildinfo_bi(bi: str):
    bifdir: str = server_app.config['PS']['BUILDINFODIR']
    return flask__send_from_directory(bifdir, bi)

@server_app.route("/build", methods=["GET"])
def build():
    bifdir: str = server_app.config['PS']['BUILDINFODIR']
    stagedir: str = server_app.config['PS']['STAGEDIR']
    deploy: str = server_app.config['PS']['DEPLOYSCRIPT']
    try:
        p0: subprocess.CompletedProcess = subprocess__run([deploy, '--build'], timeout=300, check=True, capture_output=True, text=True)
        p0.check_returncode()
    except:
        raise RuntimeError(p0)
    write_next(pathlib__Path(bifdir), p0.stdout)
    ts: str = timestamp__get_latest_str(stagedir)
    return f'''<p>Timestamp: <b>{ts}</b></p>'''

@server_app.route("/commit", methods=["GET"])
def commit():
    stagedir: str = server_app.config['PS']['STAGEDIR']
    repodir: str = server_app.config['PS']['REPODIR']
    coor__execself('refs/heads/master', stagedir, repodir)
    return f'''okay'''

@server_app.route("/status", methods=["GET"])
def status():
    deploy: str = server_app.config['PS']['DEPLOYSCRIPT']
    status: typing.List[str] = []
    try:
        p0: subprocess.CompletedProcess = subprocess__run([deploy, '--info-list'], timeout=300, capture_output=True, text=True)
        p0.check_returncode()
    except:
        raise RuntimeError(p0)
    infos: typing.List[str] = p0.stdout.split()
    try:
        for i in infos:
            p0: subprocess.CompletedProcess = subprocess__run([deploy, '--info', i], timeout=300, capture_output=True, text=True)
            p0.check_returncode()
            status.append(f'''
            <div style="border: solid; margin: 2rem 0rem 2rem 0rem;">
            <div>{i}</div>
            <pre style="border: solid; margin: 0rem">{p0.stdout}</pre>
            </div>
            ''')
    except:
        raise RuntimeError(p0)
    return f'''
<p style="border: solid;">Available: <b>{', '.join(infos)}</b></p>
<p>{''.join(status)}</p>
'''

@server_app.route("/", methods=["GET"])
def index():
    stagedir: str = server_app.config['PS']['STAGEDIR']
    ts: str = timestamp__get_latest_str(stagedir)
    return f'''
<p>Timestamp: <b>{ts}</b></p>
<p><a href="{ps_url_for('buildinfo')}">BuildInfo</a></p>
<p><a href="{ps_url_for('build')}">Build</a></p>
<p><a href="{ps_url_for('commit')}">Commit</a></p>
<p><a href="{ps_url_for('status')}">Status</a></p>
'''

def server_config_flask_and_run(_config: dict):
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
