#!/bin/bash

set -e
set -x

uname -a
python -c "import sys; print(sys.version)"

if [[ "$(uname -s)" == 'Darwin' ]]; then
    brew update || brew update
    brew outdated pyenv || brew upgrade pyenv
    brew install pyenv-virtualenv

    if which pyenv > /dev/null; then
        eval "$(pyenv init -)"
    fi

    case "${PYVER}" in
        # py26)
        #     pyenv install 2.6.9
        #     pyenv virtualenv 2.6.9 pysendfile
        #     ;;
        py27)
            pyenv install 2.7.10
            pyenv virtualenv 2.7.10 pysendfile
            ;;
        # py32)
        #     pyenv install 3.2.6
        #     pyenv virtualenv 3.2.6 pysendfile
        #     ;;
        # py33)
        #     pyenv install 3.3.6
        #     pyenv virtualenv 3.3.6 pysendfile
        #     ;;
        py34)
            pyenv install 3.4.3
            pyenv virtualenv 3.4.3 pysendfile
            ;;
    esac
    pyenv rehash
    pyenv activate pysendfile
fi

if [[ $TRAVIS_PYTHON_VERSION == '2.5' ]]; then pip install unittest2; fi
if [[ $TRAVIS_PYTHON_VERSION == '2.6' ]]; then pip install unittest2; fi
pip install flake8
python setup.py build
python setup.py install
