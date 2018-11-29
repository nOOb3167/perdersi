from confaux import sub as confaux__sub
v = {
    'STAGEDIR': '/usr/local/perdersi/stage',
    'CMAKE_PROG': '/cygdrive/e/TestM/CMake/bin/cmake'
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
    'STAGEDIR': '{STAGEDIR}',
    'REPODIR': '/usr/local/perdersi/repo_s',
    'WIN': {
        'CMAKE': '{CMAKE_PROG} \
            -S E:/prog/perdersi/perdersi \
            -B E:/prog/perdersi/PerdersiBuild \
            -G "Visual Studio 15 2017 Win64" \
            -DBOOST_ROOT=E:/prog/boost_1_68_0 \
            -DLibGit2_ROOT=E:/prog/perdersi/LibGit2Inst \
            -DSFML_DIR=E:/prog/perdersi/SFMLInst/lib/cmake/SFML \
            -DPython3_ROOT_DIR=C:/Users/Andrej/AppData/Local/Programs/Python/Python37-32 \
            -DPS_MARKER_REMOTE=ON \
            -DPS_CONFIG_DEPLOY_PYTHON_MODULE_NAME=config_deploy_andrej',
        'BUILD_MARKER': '{SSH_PROG} Andrej@localhost "cat e:/prog/perdersi/PerdersiBuild/ps_marker_remote | grep ON"',
        'BUILD': '{SSH_PROG} Andrej@localhost "bash -c \\"{CMAKE_PROG} --build e:/prog/perdersi/PerdersiBuild --target INSTALL\\""',
        'RSYNC': '{RSYNC_PROG} Andrej@localhost:/cygdrive/e/prog/perdersi/PerdersiInst/ {STAGEDIR}',
    },
}
confaux__sub(config, v)
