import pytest

def pytest_addoption(parser):
    parser.addoption("--customopt_debug_wait")
    parser.addoption("--customopt_python_exe")
    parser.addoption("--customopt_tupdater2_exe")
    parser.addoption("--customopt_tupdater3_exe")
    parser.addoption("--customopt_updater_exe")

@pytest.fixture(scope="session")
def customopt_debug_wait(request):
    return request.config.getoption("--customopt_debug_wait")

@pytest.fixture(scope="session")
def customopt_python_exe(request):
    return request.config.getoption("--customopt_python_exe")

@pytest.fixture(scope="session")
def customopt_tupdater2_exe(request):
    return request.config.getoption("--customopt_tupdater2_exe")

@pytest.fixture(scope="session")
def customopt_tupdater3_exe(request):
    return request.config.getoption("--customopt_tupdater3_exe")

@pytest.fixture(scope="session")
def customopt_updater_exe(request):
    return request.config.getoption("--customopt_updater_exe")
