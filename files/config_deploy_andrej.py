from confaux import sub as confaux__sub
from confaux import cygpath as confaux__cygpath
v = {
    'BASEP_WIN': 'e:/prog/perdersi',
    'BASEP_CYG': confaux__cygpath('e:/prog/perdersi'),
    'STAGEDIR': '/usr/local/perdersi/stage',
    'CMAKE_PROG': '/cygdrive/e/TestM/CMake/bin/cmake',
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
        'XSFER': r'{RSYNC_PROG} /root/gittest/tmp00/ Andrej@localhost:{BASEP_CYG}/RemPerdersiSrc',
        'CMAKE': r'{SSH_PROG} Andrej@localhost "bash -c \"{CMAKE_PROG} \
            -S {BASEP_WIN}/RemPerdersiSrc \
            -B {BASEP_WIN}/RemPerdersiBuild \
            -G \\\"Visual Studio 15 2017 Win64\\\" \
            -DCMAKE_INSTALL_PREFIX={BASEP_WIN}/RemPerdersiInst/\
            -DBOOST_ROOT=E:/prog/boost_1_68_0 \
            -DLibGit2_ROOT=E:/prog/perdersi/LibGit2Inst \
            -DSFML_DIR=E:/prog/perdersi/SFMLInst/lib/cmake/SFML \
            -DPython3_ROOT_DIR=C:/Users/Andrej/AppData/Local/Programs/Python/Python37-32 \
            -DPS_MARKER_REMOTE=ON \
            -DPS_CONFIG_DEPLOY_PYTHON_MODULE_NAME=config_deploy_andrej\""',
        'BUILD_MARKER': '{SSH_PROG} Andrej@localhost "cat {BASEP_WIN}/RemPerdersiBuild/ps_marker_remote | grep ON"',
        'BUILD': '{SSH_PROG} Andrej@localhost "bash -c \\"{CMAKE_PROG} --build {BASEP_WIN}/RemPerdersiBuild --target INSTALL\\""',
        'RSYNC': '{RSYNC_PROG} Andrej@localhost:{BASEP_CYG}/RemPerdersiInst/ {STAGEDIR}',
    },
}
confaux__sub(config, v)
