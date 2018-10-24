import pytest

def pytest_addoption(parser):
    parser.addoption("--customopt_python_exe")
    parser.addoption("--customopt_updater_exe")

@pytest.fixture
def customopt_python_exe(request):
    return request.config.getoption("--customopt_python_exe")

@pytest.fixture
def customopt_updater_exe(request):
    return request.config.getoption("--customopt_updater_exe")
