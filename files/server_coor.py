from timestamp import get_latest_str as timestamp__get_latest_str
import flask
from base64 import b32encode as base__b32encode
from json import loads as json__loads
from flask import Flask as flask__Flask
from os import urandom as os__urandom
from os import environ as os__environ

confdict = dict

server_app: flask.Flask = flask__Flask(__name__, static_url_path = "")

@server_app.route("/", methods=["GET"])
def index():
    q = timestamp__get_latest_str('/root/gittest')
    return f'''
        hello world {q}
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
