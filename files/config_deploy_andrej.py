from confaux import sub as confaux__sub
v = {
    'SSH_PROG': 'ssh -p 6623 -o StrictHostKeyChecking=no',
    'RSYNC_PROG': 'rsync -r -e "{SSH_PROG}"',
}
confaux__sub(v, v)
config = {
    'LISTEN_HOST': 'localhost.localdomain',
    'LISTEN_PORT': '5202',
    'ORIGIN_DOMAIN_APP': 'localhost.localdomain',
    'ORIGIN_DOMAIN_API': 'api.localhost.localdomain',
    'TESTING': False,
    'STAGEDIR': '/usr/local/perdersi/stage',
    'REPODIR': '/usr/local/perdersi/repo_s',
    'WIN': {
        'BUILD_MARKER': '{SSH_PROG} Andrej@localhost "cat e:/prog/perdersi/PerdersiBuild/ps_marker_remote | grep ON"',
        'BUILD': '{SSH_PROG} Andrej@localhost "bash -c \\"/cygdrive/e/TestM/CMake/bin/cmake --build e:/prog/perdersi/PerdersiBuild --target INSTALL\\""',
        'RSYNC': '{RSYNC_PROG} Andrej@localhost:/cygdrive/e/prog/perdersi/PerdersiInst/ /usr/local/perdersi/stage/',
    },
}
confaux__sub(config, v)
