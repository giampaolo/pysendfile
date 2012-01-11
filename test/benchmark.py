#!/usr/bin/env python
# $Id$

"""
A simle benchmark script which compares plain send() and sendfile()
performances in terms of CPU time spent and bytes transmitted in
one second.

This is what I get on my Linux 2.6.38 box, AMD dual core 1.6 GHz:

send()
  cpu:     28.41 usec/pass
  rate:   362.13 MB/sec

sendfile()
  cpu:     11.25 usec/pass
  rate:   848.56 MB/sec

Works with both python 2.X and 3.X.
"""

import socket
import os
import errno
import timeit
import time
import atexit
import sys
import optparse
import threading
import itertools
import signal
from multiprocessing import Process

from sendfile import sendfile


# overridable defaults

HOST = "127.0.0.1"
PORT = 8022
BIGFILE = "$testfile1"
BIGFILE_SIZE = 1024 * 1024 * 1024 # 1 GB
BUFFER_SIZE = 65536

def print_(s, hilite=False):
    if hilite:
        bold = '1'
        s = '\x1b[%sm%s\x1b[0m' % (';'.join([bold]), s)
    sys.stdout.write(s + "\n")
    sys.stdout.flush()

# python 3 compatibility layer
def b(s):
    return bytes(s, 'ascii') if sys.version_info >= (3,) else s

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


class Spinner(threading.Thread):

    def run(self):
        self._exit = False
        self._spinner = itertools.cycle('-\|/')
        while not self._exit:
            sys.stdout.write(next(self._spinner) + "\b")
            sys.stdout.flush()
            time.sleep(.1)

    def stop(self):
        self._exit = True
        self.join()
    

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
    use_sendfile decides whether using sendfile() or plain send().
    If keep_sending is True restart sending file when EOF is reached.
    """
    sock = socket.socket()
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((HOST, PORT))
    sock.listen(1)
    conn, addr = sock.accept()
    sock.close()
    file = open(BIGFILE, 'rb')

    def on_exit(signum, fram):
        file.close(); 
        conn.close()
        sys.exit(0)
    signal.signal(signal.SIGTERM, on_exit) 
    signal.signal(signal.SIGINT, on_exit) 

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
                if not sent:
                    # EOF
                    if keep_sending:
                        offset = 0
                        continue
                    else:
                        break
                else:
                    offset += sent


def main():
    parser = optparse.OptionParser()
    parser.add_option('-k', '--keepfile', action="store_true", default=False,
                      help="do not remove test file on exit")
    options, args = parser.parse_args()
    if not options.keepfile:
        atexit.register(lambda: safe_remove(BIGFILE))

    if not os.path.exists(BIGFILE) or os.path.getsize(BIGFILE) < BIGFILE_SIZE:
        print_("creating big file...")
        create_file(BIGFILE, BIGFILE_SIZE)
    print_("starting benchmark...")

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

    print_(" ")
    print_("send()", hilite=True)
    print_("  cpu:   %7.2f usec/pass" % (1000000 * t2 / 100000))
    print_("  rate:  %7.2f MB/sec" % round(bytes2 / 1024.0 / 1024.0, 2))
    print_("")
    print_("sendfile()", hilite=True)
    print_("  cpu:   %7.2f usec/pass" % (1000000 * t1 / 100000))
    print_("  rate:  %7.2f MB/sec" % round(bytes1 / 1024.0 / 1024.0, 2))

if __name__ == '__main__':
    s = Spinner()
    s.start()
    try:
        main()
    finally:
        s.stop()
