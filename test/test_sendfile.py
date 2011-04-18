#!/usr/bin/env python
#
# $Id$
#

import unittest
import os
import sys
import socket
import asyncore
import asynchat
import threading
import errno
import time
import atexit
import warnings

import sendfile

PY3 = sys.version_info >= (3,)

def _bytes(x):
    if PY3:
        return bytes(x, 'ascii')
    return x

TESTFN = "$testfile"
TESTFN2 = TESTFN + "2"
TESTFN3 = TESTFN + "3"
DATA = _bytes("12345abcde" * 1024 * 1024)  # 10 Mb
HOST = '127.0.0.1'
BIGFILE_SIZE = 2500000000  # > 2GB file (2GB = 2147483648 bytes)
if "sunos" not in sys.platform:
    SUPPORT_HEADER_TRAILER = True
else:
    SUPPORT_HEADER_TRAILER = False


class Handler(asynchat.async_chat):

    def __init__(self, conn):
        asynchat.async_chat.__init__(self, conn)
        self.in_buffer = []
        self.closed = False
        self.push(_bytes("220 ready\r\n"))

    def handle_read(self):
        data = self.recv(4096)
        self.in_buffer.append(data)

    def get_data(self):
        return _bytes('').join(self.in_buffer)

    def handle_close(self):
        self.close()

    def close(self):
        asynchat.async_chat.close(self)
        self.closed = True

    def handle_error(self):
        raise


class NoMemoryHandler(Handler):
    # same as above but doesn't store received data in memory
    ac_in_buffer_size = 65536

    def __init__(self, conn):
        Handler.__init__(self, conn)
        self.in_buffer_len = 0

    def handle_read(self):
        data = self.recv(self.ac_in_buffer_size)
        self.in_buffer_len += len(data)

    def get_data(self):
        raise NotImplementedError


class Server(asyncore.dispatcher, threading.Thread):

    handler = Handler

    def __init__(self, address):
        threading.Thread.__init__(self)
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.bind(address)
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

    def wait(self):
        # wait for handler connection to be closed, then stop the server
        while not getattr(self.handler_instance, "closed", True):
            time.sleep(0.001)
        self.stop()

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
        self.handler_instance = self.handler(conn)

    def handle_connect(self):
        self.close()
    handle_read = handle_connect

    def writable(self):
        return 0

    def handle_error(self):
        raise


def sendfile_wrapper(sock, file, offset, nbytes, header="", trailer=""):
    """A higher level wrapper representing how an application is
    supposed to use sendfile().
    """
    while 1:
        try:
            if SUPPORT_HEADER_TRAILER:
                return sendfile.sendfile(sock, file, offset, nbytes, header,
                                         trailer)
            else:
                return sendfile.sendfile(sock, file, offset, nbytes)
        except OSError:
            err = sys.exc_info()[1]
            if err.errno == errno.EAGAIN:  # retry
                continue
            raise


class TestSendfile(unittest.TestCase):

    def setUp(self):
        self.server = Server((HOST, 0))
        self.server.start()
        self.client = socket.socket()
        self.client.connect((self.server.host, self.server.port))
        self.client.settimeout(1)
        # synchronize by waiting for "220 ready" response
        self.client.recv(1024)
        self.sockno = self.client.fileno()
        self.file = open(TESTFN, 'rb')
        self.fileno = self.file.fileno()

    def tearDown(self):
        if os.path.isfile(TESTFN2):
            os.remove(TESTFN2)
        self.file.close()
        self.client.close()
        if self.server.running:
            self.server.stop()

    def test_send_whole_file(self):
        # normal send
        total_sent = 0
        offset = 0
        nbytes = 4096
        while 1:
            sent = sendfile_wrapper(self.sockno, self.fileno, offset, nbytes)
            if sent == 0:
                break
            total_sent += sent
            offset += sent
            self.assertTrue(sent <= nbytes)
            self.assertEqual(offset, total_sent)

        self.assertEqual(total_sent, len(DATA))
        self.client.close()
        if "sunos" in sys.platform:
            time.sleep(.1)
        self.server.wait()
        data = self.server.handler_instance.get_data()
        self.assertEqual(hash(data), hash(DATA))

    def test_send_from_certain_offset(self):
        # start sending a file at a certain offset
        total_sent = 0
        offset = int(len(DATA) / 2)
        nbytes = 4096
        while 1:
            sent = sendfile_wrapper(self.sockno, self.fileno, offset, nbytes)
            if sent == 0:
                break
            total_sent += sent
            offset += sent
            self.assertTrue(sent <= nbytes)

        self.client.close()
        if "sunos" in sys.platform:
            time.sleep(.1)
        self.server.wait()
        data = self.server.handler_instance.get_data()
        expected = DATA[int(len(DATA) / 2):]
        self.assertEqual(total_sent, len(expected))
        self.assertEqual(hash(data), hash(expected))

    if SUPPORT_HEADER_TRAILER:
        def test_header(self):
            total_sent = 0
            header = _bytes("x") * 512
            sent = sendfile.sendfile(self.sockno, self.fileno, 0, 4096,
                                     header=header)
            total_sent += sent
            offset = 4096
            nbytes = 4096
            while 1:
                sent = sendfile_wrapper(self.sockno, self.fileno, offset, nbytes)
                if sent == 0:
                    break
                offset += sent
                total_sent += sent

            expected_data = header + DATA
            self.assertEqual(total_sent, len(expected_data))
            self.client.close()
            self.server.wait()
            data = self.server.handler_instance.get_data()
            self.assertEqual(hash(data), hash(expected_data))

        def test_trailer(self):
            f = open(TESTFN2, 'wb')
            f.write(_bytes("abcde"))
            f.close()
            f = open(TESTFN2, 'rb')
            sendfile.sendfile(self.sockno, f.fileno(), 0, 4096,
                              trailer=_bytes("12345"))
            self.client.close()
            self.server.wait()
            data = self.server.handler_instance.get_data()
            self.assertEqual(data, _bytes("abcde12345"))

    def test_non_socket(self):
        fd_in = open(TESTFN, 'rb')
        fd_out = open(TESTFN2, 'wb')
        try:
            sendfile.sendfile(fd_in.fileno(), fd_out.fileno(), 0, 4096)
        except OSError:
            err = sys.exc_info()[1]
            self.assertEqual(err.errno, errno.EBADF)
        else:
            self.fail("exception not raised")

    if sys.platform.startswith('freebsd'):
        def test_send_whole_file(self):
            # On Mac OS X and FreeBSD, a value of 0 for nbytes
            # specifies to send until EOF is reached.
            # OSX implementation is broken though.
            ret = sendfile_wrapper(self.sockno, self.fileno, 0, 0)
            self.client.close()
            self.server.wait()
            data = self.server.handler_instance.get_data()
            self.assertEqual(hash(data), hash(DATA))

    if hasattr(sendfile, "SF_NODISKIO"):
        def test_flags(self):
            try:
                sendfile.sendfile(self.sockno, self.fileno, 0, 4096,
                                  flags=sendfile.SF_NODISKIO)
            except OSError:
                err = sys.exc_info()[1]
                if err.errno not in (errno.EBUSY, errno.EAGAIN):
                    raise

    # --- corner cases

    def test_offset_overflow(self):
        # specify an offset > file size
        offset = len(DATA) + 4096
        sent = sendfile.sendfile(self.sockno, self.fileno, offset, 4096)
        self.assertEqual(sent, 0)
        self.client.close()
        self.server.wait()
        data = self.server.handler_instance.get_data()
        self.assertEqual(data, _bytes(''))

    if "sunos" not in sys.platform:
        def test_invalid_offset(self):
            try:
                sendfile.sendfile(self.sockno, self.fileno, -1, 4096)
            except OSError:
                err = sys.exc_info()[1]
                self.assertEqual(err.errno, errno.EINVAL)
            else:
                self.fail("exception not raised")

    def test_small_file(self):
        data = _bytes('foo bar')
        f = open(TESTFN2, 'wb')
        f.write(data)
        f.close()
        f = open(TESTFN2, 'rb')

        sendfile_wrapper(self.sockno, f.fileno(), 0, 4096)
        self.client.close()
        if "sunos" in sys.platform:
            time.sleep(.1)
        self.server.wait()
        data_sent = self.server.handler_instance.get_data()
        self.assertEqual(data_sent, data)

    def test_small_file_and_offset_overflow(self):
        data = _bytes('foo bar')
        f = open(TESTFN2, 'wb')
        f.write(data)
        f.close()
        f = open(TESTFN2, 'rb')

        sendfile_wrapper(self.sockno, f.fileno(), 4096, 4096)
        self.client.close()
        self.server.wait()
        data_sent = self.server.handler_instance.get_data()
        self.assertEqual(data_sent, _bytes(''))

    def test_empty_file(self):
        data = _bytes('')
        f = open(TESTFN2, 'wb')
        f.write(data)
        f.close()
        f = open(TESTFN2, 'rb')

        sendfile_wrapper(self.sockno, f.fileno(), 0, 4096)
        self.client.close()
        self.server.wait()
        data_sent = self.server.handler_instance.get_data()
        self.assertEqual(data_sent, data)


class RepeatedTimer:

    def __init__(self, timeout, fun):
        self.timeout = timeout
        self.fun = fun

    def start(self):
        def main():
            self.fun()
            self.start()
        self.timer = threading.Timer(1, main)
        self.timer.start()

    def stop(self):
        self.timer.cancel()


class TestLargeFile(unittest.TestCase):

    def setUp(self):
        self.server = Server((HOST, 0))
        self.server.handler = NoMemoryHandler
        self.server.start()
        self.client = socket.socket()
        self.client.connect((self.server.host, self.server.port))
        self.client.settimeout(1)
        # synchronize by waiting for "220 ready" response
        self.client.recv(1024)
        self.sockno = self.client.fileno()
        sys.stdout.write("\ncreating file:\n")
        sys.stdout.flush()
        self.create_file()
        self.file = open(TESTFN3, 'rb')
        self.fileno = self.file.fileno()
        sys.stdout.write("\nstarting transfer:\n")
        sys.stdout.flush()

    def tearDown(self):
        #if os.path.isfile(TESTFN3):
        #    os.remove(TESTFN3)
        if hasattr(self, 'file'):
            self.file.close()
        self.client.close()
        if self.server.running:
            self.server.stop()

    def print_percent(self, a, b):
        percent = ((a * 100) / b)
        sys.stdout.write("\r%2d%%" % percent)
        sys.stdout.flush()

    def create_file(self):
        # XXX - temporary
        if os.path.isfile(TESTFN3):
            return
        f = open(TESTFN3, 'wb')
        chunk_len = 65536
        chunk = _bytes('x' * chunk_len)
        total = 0
        timer = RepeatedTimer(1, lambda: self.print_percent(total, BIGFILE_SIZE))
        timer.start()
        try:
            try:
                while 1:
                    f.write(chunk)
                    total += chunk_len
                    if total >= BIGFILE_SIZE:
                        break
            except:
                self.tearDown()
                raise
        finally:
            f.close()
            timer.stop()

    def test_big_file(self):
        total_sent = 0
        offset = 0
        nbytes = 65536
        file_size = os.path.getsize(TESTFN3)
        timer = RepeatedTimer(1, lambda: self.print_percent(total_sent,
                                                            file_size))
        timer.start()
        try:
            try:
                while 1:
                    sent = sendfile_wrapper(self.sockno, self.fileno, offset,
                                            nbytes)
                    if sent == 0:
                        break
                    total_sent += sent
                    offset += sent
                    self.assertTrue(sent <= nbytes)
                    self.assertEqual(offset, total_sent)
            except:
                print
                raise
        finally:
            print
            timer.stop()

        self.assertEqual(total_sent, file_size)
        self.client.close()
        if "sunos" in sys.platform:
            time.sleep(1)
        self.server.wait()
        data_len = self.server.handler_instance.in_buffer_len
        file_size = os.path.getsize(TESTFN3)
        self.assertEqual(file_size, data_len)


def test_main():

    def cleanup():
        if os.path.isfile(TESTFN):
            os.remove(TESTFN)
        if os.path.isfile(TESTFN2):
            os.remove(TESTFN2)

    def has_large_file_support():
        # taken from Python's Lib/test/test_largefile.py
        f = open(TESTFN, 'wb', buffering=0)
        try:
            f.seek(BIGFILE_SIZE)
            # seeking is not enough of a test: you must write and flush too
            f.write(_bytes('x'))
            f.flush()
        except (IOError, OverflowError):
            f.close()
            return False
        else:
            f.close()
            return True

    test_suite = unittest.TestSuite()
    test_suite.addTest(unittest.makeSuite(TestSendfile))
    if has_large_file_support():
        test_suite.addTest(unittest.makeSuite(TestLargeFile))
    else:
        atexit.register(warnings.warn, "couldn't run large file test because "
                  "filesystem does not have largefile support.", RuntimeWarning)
    cleanup()
    f = open(TESTFN, "wb")
    f.write(DATA)
    f.close()
    atexit.register(cleanup)
    unittest.TextTestRunner(verbosity=2).run(test_suite)

if __name__ == '__main__':
    test_main()

