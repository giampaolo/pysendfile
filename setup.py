#!/usr/bin/env python
# $Id$

import sys
from distutils.core import setup, Extension

if 'sunos' in sys.platform:
    libraries = ["sendfile"]
else:
    libraries = []

def main():
    setup(name = 'py-sendfile',
          version = '2.0.0',
          description = 'A Python interface to sendfile(2)',
          url = 'http://code.google.com/p/py-sendfile/',
          classifiers = [
              'Development Status :: 4 - Beta',
              'Intended Audience :: Developers',
              'Operating System :: POSIX :: Linux',
              'Operating System :: MacOS :: MacOS X',
              'Operating System :: POSIX :: BSD :: FreeBSD',
              'Operating System :: POSIX :: AIX',
              'Programming Language :: C',
              'Programming Language :: Python :: 2.4',
              'Programming Language :: Python :: 2.5',
              'Programming Language :: Python :: 2.6',
              'Programming Language :: Python :: 2.7',
              'Programming Language :: Python :: 3',
              'Programming Language :: Python :: 3.0',
              'Programming Language :: Python :: 3.1',
              'Programming Language :: Python :: 3.2',
              'Topic :: System :: Networking',
              'Topic :: System :: Operating System',
              'Topic :: Internet :: File Transfer Protocol (FTP)',
              'Topic :: Internet :: WWW/HTTP',
              'License :: OSI Approved :: GNU Library or Lesser General ' \
                                          'Public License (LGPL)',
               ],
          ext_modules = [Extension('sendfile',
                                   sources=['sendfilemodule.c'],
                                   libraries=libraries)],
          )

if __name__ == '__main__':
    main()
    # this will cause NotImplementedError to be raised if platform
    # is not supported
    import sendfile

