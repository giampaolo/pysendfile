.. image:: https://api.travis-ci.org/giampaolo/pysendfile.png?branch=master
    :target: https://travis-ci.org/giampaolo/pysendfile
    :alt: Linux tests (Travis)

.. image:: https://img.shields.io/pypi/v/pysendfile.svg
    :target: https://pypi.python.org/pypi/pysendfile/
    :alt: Latest version

.. image:: https://img.shields.io/github/stars/giampaolo/pysendfile.svg
    :target: https://github.com/giampaolo/pysendfile/
    :alt: Github stars

.. image:: https://img.shields.io/pypi/l/pysendfile.svg
    :target: https://pypi.python.org/pypi/pysendfile/
    :alt: License

===========
Quick links
===========

- `Home page <https://github.com/giampaolo/pysendfile>`_
- `Mailing list <http://groups.google.com/group/py-sendfile>`_
- `Blog <http://grodola.blogspot.com/search/label/pysendfile>`_
- `What's new <https://github.com/giampaolo/pysendfile/blob/master/HISTORY.rst>`_

=====
About
=====

`sendfile(2) <http://linux.die.net/man/2/sendfile>`__ is a system call which
provides a "zero-copy" way of copying data from one file descriptor to another
(a socket). The phrase "zero-copy" refers to the fact that all of the copying
of data between the two descriptors is done entirely by the kernel, with no
copying of data into userspace buffers. This is particularly useful when
sending a file over a socket (e.g. FTP).
The normal way of sending a file over a socket involves reading data from the
file into a userspace buffer, then write that buffer to the socket via
`send() <http://docs.python.org/library/socket.html#socket.socket.send>`__ or
`sendall() <http://docs.python.org/library/socket.html#socket.socket.sendall>`__:

.. code-block:: python

    # how a file is typically sent

    import socket

    file = open("somefile", "rb")
    sock = socket.socket()
    sock.connect(("127.0.0.1", 8021))

    while True:
        chunk = file.read(65536)
        if not chunk:
            break  # EOF
        sock.sendall(chunk)

This copying of the data twice (once into the userland buffer, and once out
from that userland buffer) imposes some performance and resource penalties.
`sendfile(2) <http://linux.die.net/man/2/sendfile>`__ syscall avoids these
penalties by avoiding any use of userland buffers; it also results in a single
system call (and thus only one context switch), rather than the series of
`read(2) <http://linux.die.net/man/2/read>`__ /
`write(2) <http://linux.die.net/man/2/write>`__ system calls (each system call
requiring a context switch) used internally for the data copying.

.. code-block:: python

    import socket
    import os
    from sendfile import sendfile

    file = open("somefile", "rb")
    blocksize = os.path.getsize("somefile")
    sock = socket.socket()
    sock.connect(("127.0.0.1", 8021))
    offset = 0

    while True:
        sent = sendfile(sock.fileno(), file.fileno(), offset, blocksize)
        offset += sent
        if sent == 0:
            break  # EOF

==================
A simple benchmark
==================

This `benchmark script <https://github.com/giampaolo/pysendfile/blob/master/test/benchmark.py>`__
implements the two examples above and compares plain socket.send() and
sendfile() performances in terms of CPU time spent and bytes transmitted per
second resulting in sendfile() being about **2.5x faster**. These are the
results I get on my Linux 2.6.38 box, AMD dual-core 1.6 GHz:

*send()*

+---------------+-----------------+
| CPU time      | 28.84 usec/pass |
+---------------+-----------------+
| transfer rate | 359.38 MB/sec   |
+---------------+-----------------+

*sendfile()*

+---------------+-----------------+
| CPU time      | 11.28 usec/pass |
+---------------+-----------------+
| transfer rate | 860.88 MB/sec   |
+---------------+-----------------+

===========================
When do you want to use it?
===========================

Basically any application sending files over the network can take advantage of
sendfile(2). HTTP and FTP servers are a typical example.
`proftpd <http://www.proftpd.org/>`__ and
`vsftpd <https://security.appspot.com/vsftpd.html>`__ are known to use it, so is
`pyftpdlib <http://code.google.com/p/pyftpdlib/>`__.

=================
API documentation
=================

sendfile module provides a single function: sendfile().

- ``sendfile.sendfile(out, in, offset, nbytes, header="", trailer="", flags=0)``

  Copy *nbytes* bytes from file descriptor *in* (a regular file) to file
  descriptor *out* (a socket) starting at *offset*. Return the number of
  bytes just being sent. When the end of file is reached return 0.
  On Linux, if *offset* is given as *None*, the bytes are read from the current
  position of *in* and the position of *in* is updated.
  *headers* and *trailers* are strings that are written before and after the
  data from *in* is written. In cross platform applications their usage is
  discouraged
  (`send() <http://docs.python.org/library/socket.html#socket.socket.send>`__ or
  `sendall() <http://docs.python.org/library/socket.html#socket.socket.sendall>`__
  can be used instead). On Solaris, *out* may be the file descriptor of a
  regular file or the file descriptor of a socket. On all other platforms,
  *out* must be the file descriptor of an open socket.
  *flags* argument is only supported on FreeBSD.

- ``sendfile.SF_NODISKIO``
- ``sendfile.SF_MNOWAIT``
- ``sendfile.SF_SYNC``

  Parameters for the *flags* argument, if the implementation supports it. They
  are available on FreeBSD platforms. See `FreeBSD's man sendfile(2) <http://www.freebsd.org/cgi/man.cgi?query=sendfile&sektion=2>`__.

=======================
Differences with send()
=======================

- sendfile(2) works with regular (mmap-like) files only (e.g. you can't use it
  with a `StringIO <https://docs.python.org/2/library/stringio.html>`__ object).
- Also, it must be clear that the file can only be sent "as is" (e.g. you
  can't modify the content while transmitting).
  There might be problems with non regular filesystems such as NFS,
  SMBFS/Samba and CIFS. For this please refer to
  `proftpd documentation <http://www.proftpd.org/docs/howto/Sendfile.html>`__.
- since the file is sent "as is" sendfile(2) can only be used with clear-text
  sockets (meaning `SSL <https://docs.python.org/2/library/ssl.html>`__
  is not supported).
- `OSError <http://docs.python.org/library/exceptions.html#exceptions.OSError>`__
  is raised instead of `socket.error <http://docs.python.org/library/socket.html#socket.error>`__.
  The accompaining `error codes <http://docs.python.org/library/errno.html>`__
  have the same meaning though: EAGAIN, EWOULDBLOCK, EBUSY meaning you are
  supposed to retry, ECONNRESET, ENOTCONN, ESHUTDOWN, ECONNABORTED in case of
  disconnection. Some examples:
  `benchmark script <https://github.com/giampaolo/pysendfile/blob/release-2.0.1/test/benchmark.py#L182>`__,
  `test suite <https://github.com/giampaolo/pysendfile/blob/release-2.0.1/test/test_sendfile.py#L202>`__,
  `pyftpdlib wrapper <http://code.google.com/p/pyftpdlib/source/browse/tags/release-0.7.0/pyftpdlib/ftpserver.py#1035>`__.

===============
Non-blocking IO
===============

- sendfile(2) can be used with non-blocking sockets, meaning if you try to
  send a chunk of data over a socket fd which is not "ready" you'll immediately
  get EAGAIN (then you can retry later by using `select()`, `epoll()` or
  whatever).
- the regular file fd, on the other hand, *can* block.

===================
Supported platforms
===================

This module works with Python versions from **2.5** to **3.X** and later on it
was integrated into Python 3 as
`os.sendfile() <https://docs.python.org/3/library/os.html#os.sendfile>`__
and `socket.socket().sendfile() <https://docs.python.org/3/library/socket.html#socket.socket.sendfile>`__
(see `bpo-10882 <http://bugs.python.org/issue10882>`__ and
`bpo-17552 <https://bugs.python.org/issue17552>`__).
The supported platforms are:

- **Linux**
- **Mac OSX**
- **FreeBSD**
- **Dragon Fly BSD**
- **Sun OS**
- **AIX** (not properly tested)

=======
Support
=======

Feel free to mail me at *g.rodola [AT] gmail [DOT] com* or post on the the
mailing list: http://groups.google.com/group/py-sendfile.

======
Status
======

As of now the code includes a solid `test suite <https://github.com/giampaolo/pysendfile/blob/master/test/test_sendfile.py>`__ and its ready for production use.
It's included in `pyftpdlib <http://code.google.com/p/pyftpdlib/>`__
project and used in production environments for years without any problem being
reported so far.

=======
Authors
=======

pysendfile was originally written by *Ben Woolley* including Linux, FreeBSD and
DragonFly BSD support. Later on *Niklas Edmundsson* took over maintenance and
added AIX support. After a couple of years of project stagnation
`Giampaolo Rodola' <http://grodola.blogspot.com/p/about.html>`__ took over
maintenance and rewrote it from scratch adding support for:

- Python 3
- non-blocking sockets
- `large file <http://docs.python.org/library/posix.html#large-file-support>`__ support
- Mac OSX
- Sun OS
- FreeBSD flag argument
- multiple threads (release GIL)
- a simple benchmark suite
- unit tests
- documentation
