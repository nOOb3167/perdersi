# semanage -a is faster than -m but fails on already defined
# if using -a check against the error message:
#   if [ x"$?" != x"0" ] && ! echo "$errstr" | grep "ValueError: SELinux user pspol_u is already defined"; then exit 1; fi

fail () {
    echo "ERR: $1";
    exit 1;
}

semanage user -m -R pspol_r pspol_u || fail user
semanage login -m -s pspol_u psuser || fail login

echo "SUC: Success"
