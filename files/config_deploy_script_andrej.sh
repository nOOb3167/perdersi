#!/usr/bin/env bash

set -e

CMAKE='/cygdrive/c/testM/CMake/bin/cmake'
SSH='ssh -p 6623 -o StrictHostKeyChecking=no'
RSYNC='rsync -r -e "ssh -p 6623 -o StrictHostKeyChecking=no"'

CMAKE_PARAMS="
    -S E:/prog/perdersi/RemPerdersiSrc \
    -B E:/prog/perdersi/RemPerdersiBuild \
    -G 'Visual Studio 15 2017 Win64' \
    -DCMAKE_INSTALL_PREFIX=E:/prog/perdersi/RemPerdersiInst \
    -DBOOST_ROOT=E:/prog/boost_1_68_0 \
    -DLibGit2_ROOT=E:/prog/perdersi/LibGit2Inst \
    -DSFML_DIR=E:/prog/perdersi/SFMLInst/lib/cmake/SFML \
    -DPython3_ROOT_DIR=C:/Users/Andrej/AppData/Local/Programs/Python/Python37-32 \
    -DPS_MARKER_REMOTE=ON \
    -DPS_CONFIG_DEPLOY_PYTHON_MODULE_NAME=config_deploy_andrej"
Q="\""

$RSYNC /root/gittest/tmp00/ Andrej@localhost:/cygdrive/e/prog/perdersi/RemPerdersiSrc
$SSH Andrej@localhost "bash -c $Q $CMAKE $CMAKE_PARAMS $Q"
$SSH Andrej@localhost "cat E:/prog/perdersi/RemPerdersiBuild/ps_marker_remote | grep ON"
$SSH Andrej@localhost "bash -c $Q $CMAKE --build E:/prog/perdersi/RemPerdersiBuild --target INSTALL $Q"
$RSYNC Andrej@localhost:/cygdrive/e/prog/perdersi/RemPerdersiInst/ /usr/local/perdersi/stage
