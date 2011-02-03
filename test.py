#!/usr/bin/env python
#
# $Id$
#

import unittest
import os
import sys
import socket

import sendfile

TESTFN = "$testfile"
HOST = '127.0.0.1'
#
FREEBSD = sys.platform.startswith('freebsd')
OSX = sys.platform.startswith('darwin')


class TestSendfile(unittest.TestCase):

    def tearDown(self):
        if os.path.exists(TESTFN):
            os.remove(TESTFN)

    def test_base(self):
        f = open(TESTFN, 'wb')
        f.write("testdata")
        f.close()

        serv = socket.socket()
        serv.bind((HOST, 0))
        serv.listen(5)
        cli = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        cli.connect((HOST, serv.getsockname()[1]))

        #
        fp = open(TESTFN, 'rb')
        self.assertEqual((5, 6), sendfile.sendfile(cli.fileno(), fp.fileno(), 1, 5))
        recv = serv.accept()[0]
        self.assertEqual(b"estda", recv.recv(1024))

def test_main():
    tests = [TestSendfile]
    test_suite = unittest.TestSuite()
    for test_class in tests:
        test_suite.addTest(unittest.makeSuite(test_class))
    unittest.TextTestRunner(verbosity=2).run(test_suite)


if __name__ == '__main__':
    test_main()


