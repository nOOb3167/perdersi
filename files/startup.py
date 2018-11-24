if __name__ == '__main__':
    from importlib import import_module as importlib__import_module
    from sys import argv as sys__argv
    mod: types.ModuleType = importlib_import__module(sys__argv[1])
    mod.server_run_overridable()
