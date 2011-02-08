#!/usr/bin/env python
# $Id$

"""
A simle benchmark script which compares plain send() and sendfile()
performances in terms of CPU time spent and bytes transmitted per second.

This is what I get on my Linux 2.6.35-22 box, Intel core duo 3.1 GHz:

=== send() ===
cpu:   10.98 usec/pass
speed: 918.86 Mb/sec

=== sendfile() ===
cpu:   4.94 usec/pass
speed: 1939.39 Mb/sec

Working with python 2.x only.
"""

import socket
import sys
import os
import asyncore
import asynchat
import threading
import errno
import timeit
import time
import atexit
from errno import EBADF, ECONNRESET, ENOTCONN, ESHUTDOWN, ECONNABORTED, EWOULDBLOCK

import sendfile


HOST = "127.0.0.1"
PORT = 8022
BIGFILE = "$testfile1"
BIGFILE_SIZE = 1024 * 1024 * 1024 # 1 GB
BUFFER_SIZE = 65536


class Handler(asynchat.async_chat):

    def __init__(self, conn, server):
        asynchat.async_chat.__init__(self, conn)
        self.server = server
        self.file = open(BIGFILE, "rb")
        self.sockfd = self.socket.fileno()
        self.filefd = self.file.fileno()
        self.offset = 0
        self.handle_write()

    def writable(self):
        return 1

    def handle_write(self):
        if self.server.use_sendfile:
            self.do_sendfile()
        else:
            self.do_send()

    def do_send(self):
        chunk = self.file.read(BUFFER_SIZE)
        if not chunk:
            self.handle_close()
        try:
            self.socket.sendall(chunk)
        except socket.error, err:
            if err.errno == EWOULDBLOCK:
                return
            if err.errno in (EBADF, ECONNRESET, ENOTCONN, ESHUTDOWN, ECONNABORTED):
                return self.handle_close()
            raise

    def do_sendfile(self):
        try:
            sent, _ = sendfile.sendfile(self.sockfd, self.filefd, self.offset,
                                        BUFFER_SIZE)
        except OSError, err:
            if err.errno == errno.ECONNRESET:
                # can occur on Linux
                self.handle_close()
            elif err.errno == errno.EAGAIN:
                # can occur on BSD, meaning we have to retry send data
                return
            else:
                raise
        else:
            self.offset += sent
            if sent == 0:
                self.handle_close()

    def handle_close(self):
        if self.server.keep_sending:
            self.file.seek(0)
            self.offset = 0
        else:
            self.file.close()
            self.close()

    def handle_error(self):
        raise


class Server(asyncore.dispatcher, threading.Thread):

    handler = Handler
    use_sendfile = True
    keep_sending = True

    def __init__(self):
        threading.Thread.__init__(self)
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind((HOST, PORT))
        self.listen(5)
        self.host, self.port = self.socket.getsockname()[:2]
        self.handler_instance = None
        self._active = False
        self._active_lock = threading.Lock()

    # --- public API

    @property
    def running(self):
        return self._active

    def start(self):
        assert not self.running
        self.__flag = threading.Event()
        threading.Thread.start(self)
        self.__flag.wait()

    def stop(self):
        assert self.running
        self._active = False
        self.join()

    # --- internals

    def run(self):
        self._active = True
        self.__flag.set()
        while self._active and asyncore.socket_map:
            self._active_lock.acquire()
            asyncore.loop(timeout=0.001, count=1)
            self._active_lock.release()
        asyncore.close_all()

    def handle_accept(self):
        conn, addr = self.accept()
        self.handler_instance = self.handler(conn, self)

    def handle_connect(self):
        self.close()
    handle_read = handle_connect

    def writable(self):
        return 0

    def handle_error(self):
        raise


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


def create_file(filename, size):
    f = open(filename, 'wb')
    bytes = 0
    while 1:
        data = "x" * 1024
        bytes += len(data)
        f.write(data)
        if bytes >= size:
            break
    f.close()

def start_server(use_sendfile=True, keep_sending=False):
    server = Server()
    server.use_sendfile = use_sendfile
    server.keep_sending = keep_sending
    server.start()
    return server


def main():
    atexit.register(lambda: os.remove(BIGFILE))

    if not os.path.exists(BIGFILE) or os.path.getsize(BIGFILE) < BIGFILE_SIZE:
        print "creating big file . . ."
        create_file(BIGFILE, BIGFILE_SIZE)
        print "starting benchmark . . .\n"

    # CPU time: use sendfile()
    server = start_server(use_sendfile=1)
    t1 = timeit.Timer(setup="from __main__ import Client; client = Client()",
                      stmt="client.retr()").timeit(number=1)
    server.stop()

    # CPU time: use send()
    server = start_server(use_sendfile=0)
    t2 = timeit.Timer(setup="from __main__ import Client; client = Client()",
                      stmt="client.retr()").timeit(number=1)
    server.stop()

    # MB/sec: use sendfile()
    server = start_server(use_sendfile=1, keep_sending=1)
    client = Client()
    bytes1 = client.retr_for_1_sec()
    server.stop()

    # MB/sec: use send()
    server = start_server(use_sendfile=0, keep_sending=1)
    client = Client()
    bytes2 = client.retr_for_1_sec()
    server.stop()

    print "=== send() ==="
    print "cpu:   %.2f usec/pass" % (1000000 * t2 / 100000)
    print "speed: %s Mb/sec" % round(bytes2 / 1024.0 / 1024.0, 2)
    print
    print "=== sendfile() ==="
    print "cpu:   %.2f usec/pass" % (1000000 * t1 / 100000)
    print "speed: %s Mb/sec" % round(bytes1 / 1024.0 / 1024.0, 2)
    print

if __name__ == '__main__':
    main()

