def sub(a, b):
    for v in a:
        if type(a[v]) == str:
            a[v] = a[v].format(**b)
        elif type(a[v]) == dict:
            sub(a[v], b)
