if __name__ == '__main__':
    import types
    from importlib import import_module as importlib__import_module
    from sys import argv as sys__argv
    mod: types.ModuleType = importlib__import_module(sys__argv[1])
    mod.server_run_overridable()
