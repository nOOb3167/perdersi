import re
from re import sub as re__sub
def sub(a, b):
    for v in a:
        if type(a[v]) == str:
            a[v] = a[v].format(**b)
        elif type(a[v]) == dict:
            sub(a[v], b)
def cygpath(path: str):
    # this is windows / cygwin interop
    #   subs 'X:/path' into '/cygdrive/x/path'
    def drivelower(m: re.Match):
        return '/cygdrive/' + m[1].lower() + '/' + m[2]
    return re__sub(r'^([a-zA-Z]):[/\\](.*)', drivelower, path)
