.PHONY: all ansible test clean sepol

.DEFAULT_GOAL := test

PYTHON := /usr/local/perdersi/venv/bin/python3

ifdef ANNOTATE
_ANNOTATE := --cov-report annotate:cov_annotate
else
_ANNOTATE :=
endif

all: ansible

ansible:
	ansible-playbook-3 -i ./inventory playbook.yml

test:
	$(PYTHON) -m pytest -vv --cov=. --cov-report term $(_ANNOTATE)

clean:
	rm -rf cov_annotate

sepol:
	$(MAKE) -C sepol
