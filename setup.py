from distutils.core import setup, Extension

sendfile_module = Extension('sendfile',
                    sources = ['sendfilemodule.c'])

setup (name = 'py-sendfile',
       version = '1.2.4',
       description = 'A Python interface to sendfile(2)',
       author = 'Ben Woolley',
       author_email = 'user tautolog at gmail',
       maintainer = "Giampaolo Rodola'",
       maintainer_email = 'g.rodola@gmail.com',
       url = 'http://code.google.com/p/py-sendfile/',
       license = 'LGPLv2.1+',
       classifiers = [
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)',
        'Operating System :: POSIX :: Linux',
        'Operating System :: POSIX :: BSD :: FreeBSD',
        'Operating System :: POSIX :: AIX',
        'Programming Language :: C',
        ],
       ext_modules = [sendfile_module],
       )
