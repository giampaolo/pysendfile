# Shortcuts for various tasks (UNIX only).
# To use a specific Python version run:
# $ make install PYTHON=python3.3

# You can set these variables from the command line.
PYTHON    = python
TSCRIPT   = test/test_sendfile.py

all: test

clean:
	rm -f `find . -type f -name \*.py[co]`
	rm -f `find . -type f -name \*.so`
	rm -f `find . -type f -nam1e .\*~`
	rm -f `find . -type f -name \*.orig`
	rm -f `find . -type f -name \*.bak`
	rm -f `find . -type f -name \*.rej`
	rm -rf `find . -type d -name __pycache__`
	rm -rf *.egg-info
	rm -rf *\$testfile*
	rm -rf .tox
	rm -rf build
	rm -rf dist

build: clean
	$(PYTHON) setup.py build

install: build
	if test $(PYTHON) = python2.5; then \
		$(PYTHON) setup.py install; \
	else \
		$(PYTHON) setup.py install --user; \
	fi

uninstall:
	cd ..; $(PYTHON) -m pip uninstall -y -v pysendfile; \

test: install
	$(PYTHON) $(TSCRIPT)

# requires "pip install pep8"
pep8:
	@git ls-files | grep \\.py$ | xargs pep8

# requires "pip install pyflakes"
pyflakes:
	@export PYFLAKES_NODOCTEST=1 && \
		git ls-files | grep \\.py$ | xargs pyflakes

# requires "pip install flake8"
flake8:
	@git ls-files | grep \\.py$ | xargs flake8

# upload source tarball on https://pypi.python.org/pypi/pysendfile.
upload-src: clean
	$(PYTHON) setup.py sdist upload

# git-tag a new release
git-tag-release:
	git tag -a release-`python -c "import setup; print(setup.VERSION)"` -m `git rev-list HEAD --count`:`git rev-parse --short HEAD`
