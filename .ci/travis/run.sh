#!/bin/bash

set -e
set -x

if [[ "$(uname -s)" == 'Darwin' ]]; then
    if which pyenv > /dev/null; then
        eval "$(pyenv init -)"
    fi
    pyenv activate pysendfile
fi

python setup.py build
python setup.py develop
python test/test_sendfile.py
flake8
