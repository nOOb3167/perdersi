import base64
import os

def doraise():
    raise Exception('Error')

def almost_random():
        return base64.b32encode(os.urandom(24)).decode("UTF-8")

SERVER_CONFIG_PROD = "PS_CONFIG_PROD" in os.environ and os.environ["PS_SERVER_CONFIG_PROD"]

config = {
    "LISTEN_HOST": SERVER_CONFIG_PROD and (os.environ["PS_CONFIG_LISTEN_HOST"] or doraise()) or "localhost.localdomain",
    "LISTEN_PORT": SERVER_CONFIG_PROD and (os.environ["PS_CONFIG_LISTEN_PORT"] or doraise()) or "5000",
    "ORIGIN_DOMAIN_APP": SERVER_CONFIG_PROD and (os.environ["PS_CONFIG_ORIGIN_DOMAIN_APP"] or doraise()) or "localhost.localdomain",
    "ORIGIN_DOMAIN_API": SERVER_CONFIG_PROD and (os.environ["PS_CONFIG_ORIGIN_DOMAIN_API"] or doraise()) or "api.localhost.localdomain",
    "REPO_DIR": SERVER_CONFIG_PROD and (os.environ["PS_CONFIG_REPO_DIR"] or doraise()) or "/usr/local/perdersi/repo_s"
}

try:
    from config_override import config as config_override
    for c in config_override:
        config[c] = config_override[c]
except:
    pass
