from setuptools import setup
from distutils.core import Extension

sendfile_module = Extension('sendfile',
                    sources = ['sendfilemodule.c'])

setup (name = 'py-sendfile',
       version = '1.2.4',
       description = 'A Python interface to sendfile(2)',
       author = 'Ben Woolley', 
       author_email = 'user tautolog at gmail',
       maintainer = 'Stephan Peijnik', 
       maintainer_email = 'stephan@peijnik.at',
       url = 'http://code.sp-its.at/projects/py-sendfile',
       license = 'LGPLv2.1+',
       classifiers = [
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)',
        'Operating System :: POSIX :: Linux',
        'Operating System :: POSIX :: BSD :: FreeBSD',
        'Operating System :: POSIX :: AIX',
        'Programming Language :: C',
   
        ],
       ext_modules = [sendfile_module],
       zip_safe = False,
       )
