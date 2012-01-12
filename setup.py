#!/usr/bin/env python
# $Id$

# ======================================================================
# This software is distributed under the MIT license reproduced below:
#
#     Copyright (C) 2009-2012  Giampaolo Rodola' <g.rodola@gmail.com>
#
# Permission to use, copy, modify, and distribute this software and
# its documentation for any purpose and without fee is hereby
# granted, provided that the above copyright notice appear in all
# copies and that both that copyright notice and this permission
# notice appear in supporting documentation, and that the name of
# Giampaolo Rodola' not be used in advertising or publicity pertaining to
# distribution of the software without specific, written prior
# permission.
#
# Giampaolo Rodola' DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
# NO EVENT Giampaolo Rodola' BE LIABLE FOR ANY SPECIAL, INDIRECT OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# ======================================================================

import sys
try:
    from setuptools import setup, Extension
except ImportError:
    from distutils.core import setup, Extension

if sys.version_info < (2, 5):
    sys.exit('python version not supported (< 2.5)')

if 'sunos' in sys.platform:
    libraries = ["sendfile"]
else:
    libraries = []

name = 'pysendfile'
version = '0.2.0'
download_url = "http://pysendfile.googlecode.com/files/" + name + "-" + \
                                                          version + ".tar.gz"

def main():
    setup(name=name,
          url='http://code.google.com/p/pysendfile/',
          version=version,
          description='A Python interface to sendfile(2)',
          long_description=open('README', 'r').read(),
          author='Giampaolo Rodola',
          author_email='g.rodola@gmail.com',
          download_url=download_url,
          platforms='UNIX',
          license='MIT',
          keywords=['sendfile', 'python', 'performance', 'ftp'],
          classifiers = [
              'Development Status :: 5 - Production/Stable',
              'Intended Audience :: Developers',
              'Operating System :: POSIX :: Linux',
              'Operating System :: MacOS :: MacOS X',
              'Operating System :: POSIX :: BSD',
              'Operating System :: POSIX :: BSD :: FreeBSD',
              'Operating System :: POSIX :: SunOS/Solaris',
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
              'License :: OSI Approved :: MIT License',
          ],
          ext_modules = [Extension('sendfile',
                                   sources=['sendfilemodule.c'],
                                   libraries=libraries)],
  )

main()
