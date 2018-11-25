config = {
    'LISTEN_HOST': 'localhost.localdomain',
    'LISTEN_PORT': '5202',
    'ORIGIN_DOMAIN_APP': 'localhost.localdomain',
    'ORIGIN_DOMAIN_API': 'api.localhost.localdomain',
    'TESTING': False,
    'WIN': {
        'BUILD': 'ssh -p 6623 -o StrictHostKeyChecking=no Andrej@localhost "bash -c \"/cygdrive/e/TestM/CMake/bin/cmake --build e:/prog/perdersi/PerdersiBuild --target INSTALL\""',
        'RSYNC': 'rsync -r -e "ssh -p 6623 -o StrictHostKeyChecking=no" Andrej@localhost:/cygdrive/e/prog/perdersi/PerdersiInst/ /usr/local/perdersi/deploy2/',
    },
}
