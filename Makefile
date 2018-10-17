.PHONY: all ansible test

.DEFAULT_GOAL := test

PYTHON := /usr/local/perdersi/venv/bin/python3

all: ansible

ansible:
	ansible-playbook-3 -i ./inventory playbook.yml

test:
	$(PYTHON) -m pytest -vv
