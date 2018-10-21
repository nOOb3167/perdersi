.PHONY: all ansible ansible-base ansible-se serv test clean sepol

.DEFAULT_GOAL := test

PYTHON := /usr/local/perdersi/venv/bin/python3

ifdef X
_X := -x
endif

ifdef V
_V := -vvv
endif

ifdef ANNOTATE
_ANNOTATE := --cov-report annotate:cov_annotate
endif

all: ansible

ansible: ansible-base ansible-se

ansible-base ansible-se ansible-updater: ansible-%: ansible/inventory ansible/%.yml
	ansible-playbook-3 $(_V) -i ansible/inventory ansible/$*.yml

serv:
	python3 files/config_serv.py

test:
	$(PYTHON) -m pytest -vv --cov=. --cov-report term $(_ANNOTATE) $(_X)

clean:
	rm -rf cov_annotate
