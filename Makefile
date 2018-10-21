.PHONY: all ansible ansible-base ansible-se test clean sepol

.DEFAULT_GOAL := test

PYTHON := /usr/local/perdersi/venv/bin/python3

ifdef X
_X := -x
endif

ifdef ANNOTATE
_ANNOTATE := --cov-report annotate:cov_annotate
endif

all: ansible

ansible: ansible-base ansible-se

ansible-base ansible-se: ansible-%: ansible/inventory ansible/%.yml
	ansible-playbook-3 -i ansible/inventory ansible/$*.yml

test:
	$(PYTHON) -m pytest -vv --cov=. --cov-report term $(_ANNOTATE) $(_X)

clean:
	rm -rf cov_annotate
