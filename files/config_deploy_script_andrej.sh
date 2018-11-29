#!/usr/bin/env bash

set -e

Q="\""

CMAKE="/cygdrive/e/testM/CMake/bin/cmake"
SSH="ssh -p 6623 -o StrictHostKeyChecking=no"
RSYNC="rsync -r"

CMAKE_PARAMS="-DCMAKE_INSTALL_PREFIX=E:/prog/perdersi/RemPerdersiInst \
    -S E:/prog/perdersi/RemPerdersiSrc \
    -B E:/prog/perdersi/RemPerdersiBuild \
    -G 'Visual Studio 15 2017 Win64' \
    -DBOOST_ROOT=E:/prog/boost_1_68_0 \
    -DLibGit2_ROOT=E:/prog/perdersi/LibGit2Inst \
    -DSFML_DIR=E:/prog/perdersi/SFMLInst/lib/cmake/SFML \
    -DPython3_ROOT_DIR=C:/Users/Andrej/AppData/Local/Programs/Python/Python37-32 \
    -DPS_MARKER_REMOTE=ON \
    -DPS_CONFIG_DEPLOY_PYTHON_MODULE_NAME=config_deploy_andrej"

$RSYNC -e "$SSH" /root/gittest/tmp00/ Andrej@localhost:/cygdrive/e/prog/perdersi/RemPerdersiSrc
$SSH Andrej@localhost "bash -c $Q $CMAKE $CMAKE_PARAMS $Q"
$SSH Andrej@localhost "cat E:/prog/perdersi/RemPerdersiBuild/ps_marker_remote | grep ON"
$SSH Andrej@localhost "bash -c $Q $CMAKE --build E:/prog/perdersi/RemPerdersiBuild --target INSTALL $Q"
$RSYNC -e "$SSH" Andrej@localhost:/cygdrive/e/prog/perdersi/RemPerdersiInst/ /usr/local/perdersi/stage
