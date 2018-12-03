#!/usr/bin/env bash

Q="\""

SSH_="ssh -p 6623 -o StrictHostKeyChecking=no -o ConnectTimeout=10"
SSH="$SSH_ Andrej@localhost"
RSYNC="rsync -r --delete --checksum"

CMAKE="/cygdrive/e/testM/CMake/bin/cmake"
CMAKE_PARAMS="-DCMAKE_INSTALL_PREFIX=E:/prog/perdersi/RemPerdersiInst \
    -S E:/prog/perdersi/RemPerdersiSrc \
    -B E:/prog/perdersi/RemPerdersiBuild \
    -G \"Visual Studio 15 2017 Win64\" \
    -DBOOST_ROOT=E:/prog/boost_1_68_0 \
    -DLibGit2_ROOT=E:/prog/perdersi/LibGit2Inst \
    -DSFML_DIR=E:/prog/perdersi/SFMLInst/lib/cmake/SFML \
    -DPython3_ROOT_DIR=C:/Users/Andrej/AppData/Local/Programs/Python/Python37-32 \
    -DPS_MARKER_REMOTE=ON \
    -DPS_CONFIG_DEPLOY_PYTHON_MODULE_NAME=config_deploy_andrej \
    -DPS_CONFIG_SERVER_PYTHON_MODULE_NAME=config_server_andrej \
    -DPS_CONFIG_UPDATER_PYTHON_MODULE_NAME=config_updater_andrej"
PYTHON="/cygdrive/c/Users/Andrej/AppData/Local/Programs/Python/Python37-32/python"

psexit() { echo "$*"; exit 1; }
psdate_fromunix() { date --date=@"$1"; }

case $1 in
    --build)
        $RSYNC -e "$SSH_" /root/gittest/tmp00/ Andrej@localhost:/cygdrive/e/prog/perdersi/RemPerdersiSrc || psexit transfer local remote
        $SSH "bash -l -c '$CMAKE $CMAKE_PARAMS'"                                                         || psexit cmake configure
        $SSH "cat E:/prog/perdersi/RemPerdersiBuild/ps_marker_remote | grep ON"               || psexit marker
        $SSH "bash -l -c '$CMAKE --build E:/prog/perdersi/RemPerdersiBuild --target INSTALL'" || psexit cmake build
        $RSYNC -e "$SSH_" Andrej@localhost:/cygdrive/e/prog/perdersi/RemPerdersiInst/ /usr/local/perdersi/stage || psexit transfer remote local
        ;;
    --info-list)
        echo dirs config marker repo service
        ;;
    --info)
        case $2 in
            dirs)
                d0=/cygdrive/e/prog/perdersi/RemPerdersiSrc
                d1=/cygdrive/e/prog/perdersi/RemPerdersiBuild
                d2=/cygdrive/e/prog/perdersi/RemPerdersiInst
                darray=("$d0" "$d1" "$d2")
                tsarray=($($SSH "for d in '$d0' '$d1' '$d2'; do find \"\$d\" -printf '%T@\\n' | sort -g | tail -n 1; done"))
                ynarray=($($SSH "for d in '$d0' '$d1' '$d2'; do [ -d \"\$d\" ] && echo Y || echo N; done"))
                for d in 0 1 2; do
                    echo -e ${darray[$d]} '\t' ${ynarray[$d]} '\t' $(psdate_fromunix "${tsarray[$d]}")
                done
                ;;
            config)
                $SSH "PYTHONPATH='E:/prog/perdersi/RemPerdersiSrc/files;E:/prog/perdersi/RemPerdersiBuild' $PYTHON -c \"from pprint import pprint as p; import ps_config_deploy; p(ps_config_deploy.config); import ps_config_server; p(ps_config_server.config); import ps_config_updater; p(ps_config_updater.config)\""
                ;;
            marker)
                $SSH "cat E:/prog/perdersi/RemPerdersiBuild/ps_marker_remote | grep ON"
                ;;
            repo)
                cd /usr/local/perdersi/repo_s
                sha=$(git show-ref -s refs/heads/master)
                com=$(git cat-file -p $sha)
                comdate=$(git show -s --format="%ad" $sha)
                prevstat=$(git diff --stat HEAD^..HEAD)
                echo == Latest Commit ==
                echo $sha "  " refs/heads/master "  " $comdate
                echo == Commit ==
                echo "$com"
                echo == Commit Parent Diffstat ==
                echo "$prevstat"
                ;;
            service)
                IFS=
                echo == Nginx ==
                echo "$(systemctl status nginx)"
                echo == Updater ==
                echo "$(systemctl status ps_updater)"
                echo == Coor ==
                echo "$(systemctl status ps_coor)"
                unset IFS
                ;;
            *) psexit args info ;;
        esac
        ;;
    *) psexit args ;;
esac
