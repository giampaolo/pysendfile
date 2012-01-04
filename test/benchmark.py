#!/usr/bin/env python
# $Id$

"""
A simle benchmark script which compares plain send() and sendfile()
performances in terms of CPU time spent and bytes transmitted per second.

This is what I get on my Linux 2.6.35-22 box, Intel core-duo, 3.1 GHz:

=== send() ===
cpu:   6.60 usec/pass
speed: 1527.67 Mb/sec

=== sendfile() ===
cpu:   3.47 usec/pass
speed: 2882.97 Mb/sec

Works with both python 2.X and 3.X.
"""

import socket
import os
import errno
import timeit
import time
import atexit
import sys
from multiprocessing import Process

from sendfile import sendfile


HOST = "127.0.0.1"
PORT = 8022
BIGFILE = "$testfile1"
BIGFILE_SIZE = 1024 * 1024 * 1024 # 1 GB  1073741824  # 1 GB
BUFFER_SIZE = 65536

# --- python 3 compatibility functions

def print_(s):
    sys.stdout.write(s + "\n")
    sys.stdout.flush()

def b(s):
    return bytes(s, 'ascii') if sys.version_info >= (3,) else s

# --- utils

def create_file(filename, size):
    f = open(filename, 'wb')
    bytes = 0
    chunk = b("x") * BUFFER_SIZE
    while 1:
        f.write(chunk)
        bytes += len(chunk)
        if bytes >= size:
            break
    f.close()

def safe_remove(file):
    try:
        os.remove(file)
    except OSError:
        pass


class Client:

    def __init__(self):
        self.sock = socket.socket()
        self.sock.connect((HOST, PORT))
        self.sock.settimeout(1)

    def retr(self):
        while 1:
            data = self.sock.recv(BUFFER_SIZE)
            if not data:
                break

    def retr_for_1_sec(self):
        stop_at = time.time() + 1
        bytes_recv = 0
        while stop_at > time.time():
            chunk = self.sock.recv(BUFFER_SIZE)
            if not chunk:
                assert 0
            bytes_recv += len(chunk)
        return bytes_recv

def start_server(use_sendfile, keep_sending=False):
    """A simple test server which sends a file once a client connects.
    use_sendfile decides whether using sendfile() or plain sendall()
    method.
    If keep_sending is True restart sending file when EOF is reached
    instead of disconnecting.
    """
    sock = socket.socket()
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((HOST, PORT))
    sock.listen(1)
    conn, addr = sock.accept()
    file = open(BIGFILE, 'rb')
    if not use_sendfile:
        while 1:
            chunk = file.read(BUFFER_SIZE)
            if not chunk:
                # EOF
                if keep_sending:
                    file.seek(0)
                    continue
                else:
                    break
            conn.sendall(chunk)
    else:
        offset = 0
        sockno = conn.fileno()
        fileno = file.fileno()
        while 1:
            try:
                sent = sendfile(sockno, fileno, offset, BUFFER_SIZE)
            except OSError:
                err = sys.exc_info()[1]
                if err.errno in (errno.EAGAIN, errno.EBUSY):
                    continue
                raise
            else:
                if sent == 0:
                    # EOF
                    if keep_sending:
                        offset = 0
                        continue
                    else:
                        break
                else:
                    offset += sent
    file.close()
    conn.close()


def main():
    atexit.register(lambda: safe_remove(BIGFILE))

    if not os.path.exists(BIGFILE) or os.path.getsize(BIGFILE) < BIGFILE_SIZE:
        print_("creating big file . . .")
        create_file(BIGFILE, BIGFILE_SIZE)
        print_("starting benchmark . . .")

    # CPU time: use sendfile()
    server = Process(target=start_server, kwargs={"use_sendfile":True})
    server.start()
    time.sleep(0.1)
    t1 = timeit.Timer(setup="from __main__ import Client; client = Client()",
                      stmt="client.retr()").timeit(number=1)
    server.terminate()
    server.join()

    # CPU time: use send()
    server = Process(target=start_server, kwargs={"use_sendfile":False})
    server.start()
    time.sleep(0.1)
    t2 = timeit.Timer(setup="from __main__ import Client; client = Client()",
                      stmt="client.retr()").timeit(number=1)
    server.terminate()
    server.join()

    # MB/sec: use sendfile()
    server = Process(target=start_server, kwargs={"use_sendfile":True,
                                                  "keep_sending":True})
    server.start()
    time.sleep(0.1)
    client = Client()
    bytes1 = client.retr_for_1_sec()
    server.terminate()
    server.join()

    # MB/sec: use sendfile()
    server = Process(target=start_server, kwargs={"use_sendfile":False,
                                                  "keep_sending":True})
    server.start()
    time.sleep(0.1)
    client = Client()
    bytes2 = client.retr_for_1_sec()
    server.terminate()
    server.join()

    print_("=== send() ===")
    print_("cpu:   %.2f usec/pass" % (1000000 * t2 / 100000))
    print_("speed: %s MB/sec" % round(bytes2 / 1024.0 / 1024.0, 2))
    print_("")
    print_("=== sendfile() ===")
    print_("cpu:   %.2f usec/pass" % (1000000 * t1 / 100000))
    print_("speed: %s MB/sec" % round(bytes1 / 1024.0 / 1024.0, 2))

if __name__ == '__main__':
    main()
