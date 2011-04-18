/*
 * $Id$
 */

/*
 * py-sendfile
 *
 * A Python module interface to sendfile(2)
 *
 * Original author:
 *     Copyright (C) 2005 Ben Woolley <user tautolog at gmail>
 *
 * The AIX support code is:
 *     Copyright (C) 2008,2009 Niklas Edmundsson <nikke@acc.umu.se>
 *
 * Currently maintained by Giampaolo Rodola'
 *     Copyright (C) 2011 <g.rodola@gmail.com>
 *
 *
 *  The MIT License
 *
 *  Copyright (c) <2011> <Ben Woolley, Niklas Edmundsson, Giampaolo Rodola'>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
*/

#include <Python.h>
#include <stdlib.h>

int unsupported = 0;

/* --- begin FreeBSD / Dragonfly / OSX --- */
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

// needed on OSX - Python < 2.6
#if defined(__APPLE__) && defined(_POSIX_C_SOURCE)
struct sf_hdtr {
    struct iovec *headers;
    int hdr_cnt;
    struct iovec *trailers;
    int trl_cnt;
};
#endif

static PyObject *
method_sendfile(PyObject *self, PyObject *args, PyObject *kwdict)
{
    int fd;
    int sock;
    int flags = 0;
    int ret;
    off_t offset;
    size_t nbytes;
    char * head = NULL;
    size_t head_len = 0;
    char * tail = NULL;
    size_t tail_len = 0;
    off_t sent;
    struct sf_hdtr hdtr;
    static char *keywords[] = {"out", "in", "offset", "nbytes", "header",
                               "trailer", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwdict,
#if defined(HAVE_LARGEFILE_SUPPORT)
                                     "iiLl|s#s#i:sendfile",
#else
                                     "iill|s#s#i:sendfile",
#endif
                                     keywords, &fd, &sock, &offset, &nbytes,
                                     &head, &head_len, &tail, &tail_len,
                                     &flags)) {
        return NULL;
    }

#ifdef __APPLE__
    sent = nbytes;
#endif
    if (head_len != 0 || tail_len != 0) {
        struct iovec ivh = {head, head_len};
        struct iovec ivt = {tail, tail_len};
        hdtr.headers = &ivh;
        hdtr.hdr_cnt = 1;
        hdtr.trailers = &ivt;
        hdtr.trl_cnt = 1;
        Py_BEGIN_ALLOW_THREADS
#ifdef __APPLE__
        sent += head_len;
        ret = sendfile(sock, fd, offset, &sent, &hdtr, flags);
#else
        ret = sendfile(sock, fd, offset, nbytes, &hdtr, &sent, flags);
#endif
        Py_END_ALLOW_THREADS
    }
    else {
        Py_BEGIN_ALLOW_THREADS
#ifdef __APPLE__
        ret = sendfile(sock, fd, offset, &sent, NULL, flags);
#else
        ret = sendfile(sock, fd, offset, nbytes, NULL, &sent, flags);
#endif
        Py_END_ALLOW_THREADS
    }

    if (ret < 0) {
        if ((errno == EAGAIN) || (errno == EBUSY)) {
            if (sent != 0) {
                // some data has been sent
                goto done;
            }
            else {
                // no data has been sent; upper application is supposed
                // to retry on EAGAIN or EBUSY
                PyErr_SetFromErrno(PyExc_OSError);
                return NULL;
            }
        }
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    goto done;

done:
    #if defined(HAVE_LARGEFILE_SUPPORT)
        return Py_BuildValue("L", sent);
    #else
        return Py_BuildValue("l", sent);
    #endif
}
/* --- end OSX / FreeBSD / Dragonfly --- */

/* --- begin AIX --- */
#elif defined(_AIX)
#include <sys/socket.h>

static PyObject *
method_sendfile(PyObject *self, PyObject *args)
{
    int out_fd, in_fd;
    off_t offset;
    size_t nbytes;
    char *hdr=NULL, *trail=NULL;
    int hdrsize, trailsize;
    ssize_t sts=0;
    struct sf_parms sf_iobuf;
    int rc;

    if (!PyArg_ParseTuple(args, "iiLk|s#s#",
                          &out_fd, &in_fd, &offset, &nbytes, &hdr, &hdrsize,
			  &trail, &trailsize))
        return NULL;

    if(hdr != NULL) {
        sf_iobuf.header_data = hdr;
        sf_iobuf.header_length = hdrsize;
    }
    else {
        sf_iobuf.header_data = NULL;
        sf_iobuf.header_length = 0;
    }
    if(trail != NULL) {
        sf_iobuf.trailer_data = trail;
        sf_iobuf.trailer_length = trailsize;
    }
    else {
	sf_iobuf.trailer_data = NULL;
	sf_iobuf.trailer_length = 0;
    }
    sf_iobuf.file_descriptor = in_fd;
    sf_iobuf.file_offset = offset;
    sf_iobuf.file_bytes = nbytes;

    Py_BEGIN_ALLOW_THREADS;
    do {
	    sf_iobuf.bytes_sent = 0; /* Really needed? */
            rc = send_file(&out_fd, &sf_iobuf, SF_DONT_CACHE);
	    sts += sf_iobuf.bytes_sent;
    } while( rc == 1 || (rc == -1 && errno == EINTR) );
    Py_END_ALLOW_THREADS;

    offset = sf_iobuf.file_offset;

    if (rc == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    else {
    #if defined(HAVE_LARGEFILE_SUPPORT)
        return Py_BuildValue("L", sts);
    #else
        return Py_BuildValue("l", sts);
    #endif
    }
}
/* --- end AIX --- */

/* --- start Linux --- */
#elif defined (__linux__)
#include <sys/sendfile.h>
#include <sys/socket.h>

// these are taken from linux/socket.h, for some reason including
// that file doesn't work here.
#define SOL_TCP 6
#define TCP_CORK 3

static PyObject *
method_sendfile(PyObject *self, PyObject *args, PyObject *kwdict)
{
    int out_fd, in_fd;
    off_t offset;
    size_t nbytes;
    char * head = NULL;
    size_t head_len = 0;
    char * tail = NULL;
    size_t tail_len = 0;
    int orig_cork = 1;
    int orig_cork_len = sizeof(int);
    int flags;
    int ret;
    ssize_t sent_h = 0;
    ssize_t sent_f = 0;
    ssize_t sent_t = 0;

    static char *keywords[] = {"out", "in", "offset", "nbytes", "header",
                               "trailer", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwdict,
#if defined(HAVE_LARGEFILE_SUPPORT)
                                     "iiLL|s#s#i:sendfile",
#else
                                     "iill|s#s#i:sendfile",
#endif
                                     keywords, &out_fd, &in_fd, &offset,
                                     &nbytes, &head, &head_len, &tail,
                                     &tail_len, &flags)) {
        return NULL;
    }

    if (head_len != 0 || tail_len != 0) {
        int cork = 1;
        // first, fetch the original setting
        Py_BEGIN_ALLOW_THREADS
        ret = getsockopt(out_fd, SOL_TCP, TCP_CORK,
                         (void*)&orig_cork, (socklen_t*)&orig_cork_len);
        Py_END_ALLOW_THREADS
        if (ret == -1)
            return PyErr_SetFromErrno(PyExc_OSError);
        Py_BEGIN_ALLOW_THREADS
        ret = setsockopt(out_fd, SOL_TCP, TCP_CORK, (void*)&cork, sizeof(cork));
        Py_END_ALLOW_THREADS
        if (ret == -1)
            return PyErr_SetFromErrno(PyExc_OSError);
    }

    // send header
    if (head_len != 0) {
        Py_BEGIN_ALLOW_THREADS
        sent_h = send(out_fd, head, head_len, 0);
        Py_END_ALLOW_THREADS
        if (sent_h < 0)
            return PyErr_SetFromErrno(PyExc_OSError);
        else if (sent_h == 0) {
            // A return value of 0 is supposed to be interpreted as EOF
            // being reached. We do not want that and raise EAGAIN instead.
            errno = EAGAIN;
            return PyErr_SetFromErrno(PyExc_OSError);
        }
        else if (sent_h < head_len)
            goto done;
    }

    // send file
    Py_BEGIN_ALLOW_THREADS;
    sent_f = sendfile(out_fd, in_fd, &offset, nbytes);
    Py_END_ALLOW_THREADS;
    if (sent_f == -1)
        return PyErr_SetFromErrno(PyExc_OSError);

    // send trailer
    if (tail_len != 0) {
        Py_BEGIN_ALLOW_THREADS
        sent_t = send(out_fd, tail, tail_len, 0);
        Py_END_ALLOW_THREADS
        if (sent_t < 0)
           return PyErr_SetFromErrno(PyExc_OSError);
        else if (sent_t == 0) {
            // A return value of 0 is supposed to be interpreted as EOF
            // being reached. We do not want that and raise EAGAIN instead.
            errno = EAGAIN;
            return PyErr_SetFromErrno(PyExc_OSError);
        }
    }

    goto done;

done:
#if defined(HAVE_LARGEFILE_SUPPORT)
    return Py_BuildValue("L", sent_h + sent_f + sent_t);
#else
    return Py_BuildValue("l", sent_h + sent_f + sent_t);
#endif
}
/* --- end Linux --- */

/* --- begin SUN OS --- */
#elif defined(__sun)
#include <sys/sendfile.h>

static PyObject *
method_sendfile(PyObject *self, PyObject *args, PyObject *kwdict)
{
    int out_fd;
    int in_fd;
    off_t offset;
    size_t nbytes;
    ssize_t sent;

    if (!PyArg_ParseTuple(args,
#if defined(HAVE_LARGEFILE_SUPPORT)
                          "iiLL",
#else
                          "iill",
#endif
                           &out_fd, &in_fd, &offset, &nbytes)) {
        return NULL;
    }
    sent = sendfile(out_fd, in_fd, &offset, nbytes);
    if (sent == -1)
        return PyErr_SetFromErrno(PyExc_OSError);
    return Py_BuildValue("i", sent);
}
#else
/* --- end SUN OS --- */

/* --- being not supported --- */
static PyObject *
method_sendfile(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_NotImplementedError, "platform not supported");
    return NULL;
}
#endif
/* --- end not supported --- */


static PyMethodDef
SendfileMethods[] =
{
    {"sendfile", method_sendfile,  METH_VARARGS | METH_KEYWORDS,
"sendfile(out, in, offset, nbytes, header=None, trailer=None, flags=0)\n\n"
"Return the number of bytes just being sent. When the end of file is reached "
"return 0.\n"
"headers and trailers are strings that are written before and after the data "
"from in is written\n."
"On Solaris, out may be the file descriptor of a regular file or the file "
"descriptor of a socket. On all other platforms, out must be the file "
"descriptor of an open socket.\n"
"flags argument is only supported on FreeBSD."
    },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
#endif

#if PY_MAJOR_VERSION >= 3
static int
sendfile_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int
sendfile_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef
moduledef = {
        PyModuleDef_HEAD_INIT,
        "sendfile",
        NULL,
        sizeof(struct module_state),
        SendfileMethods,
        NULL,
        sendfile_traverse,
        sendfile_clear,
        NULL
};

#define INITERROR return NULL

PyObject *
PyInit_sendfile(void)

#else
#define INITERROR return

void initsendfile(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("sendfile", SendfileMethods);
#endif
    // constants
#ifdef SF_NODISKIO
    PyModule_AddIntConstant(module, "SF_NODISKIO", SF_NODISKIO);
#endif
#ifdef SF_MNOWAIT
    PyModule_AddIntConstant(module, "SF_MNOWAIT", SF_MNOWAIT);
#endif
#ifdef SF_SYNC
    PyModule_AddIntConstant(module, "SF_SYNC", SF_SYNC);
#endif

    if (module == NULL) {
        INITERROR;
    }

    if (unsupported == 1) {
        PyErr_SetString(PyExc_NotImplementedError, "platform not supported");
    }

#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}

