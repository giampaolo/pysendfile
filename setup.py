#!/usr/bin/env python
# $Id$

from distutils.core import setup, Extension

def main():
    setup(name = 'py-sendfile',
          version = '1.2.4',
          description = 'A Python interface to sendfile(2)',
          author = 'Ben Woolley',
          author_email = 'user tautolog at gmail',
          maintainer = "Giampaolo Rodola'",
          maintainer_email = 'g.rodola@gmail.com',
          url = 'http://code.google.com/p/py-sendfile/',
          classifiers = [
              'Development Status :: 4 - Beta',
              'Intended Audience :: Developers',
              'License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)',
              'Operating System :: POSIX :: Linux',
              'Operating System :: POSIX :: BSD :: FreeBSD',
              'Operating System :: POSIX :: AIX',
              'Programming Language :: C',
              ],
          ext_modules = [Extension('sendfile',
                                   sources=['sendfilemodule.c'])],
          )

if __name__ == '__main__':
    main()
    # this will cause NotImplementedError to be raised if platform
    # is not supported
    import sendfile
