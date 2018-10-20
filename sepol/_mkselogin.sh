errstr=$(semanage login -a -s pspol_u psuser 2>&1 1>/dev/null)
err=$?
if [ x"$err" != x"0" ]; then
    errstr2=$(echo "$errstr" | grep "ValueError: SELinux user pspol_u is already defined")
    echo $? rrrr $errstr2
    exit 1;
fi

echo err $err zzzzzz $errstr
