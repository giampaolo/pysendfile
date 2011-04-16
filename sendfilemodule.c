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

/* --- begin FreeBSD / Dragonfly --- */
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>


static PyObject *
method_sendfile(PyObject *self, PyObject *args, PyObject *kwdict)
{
    int fd;
    int sock;
    int flags = 0;
    long o,n;
    char * head = NULL;
    size_t head_len = 0;
    char * tail = NULL;
    size_t tail_len = 0;
    static char *keywords[] = {"out", "in", "offset", "count", "headers",
                               "trailers", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwdict,
                                     "iill|s#s#i:sendfile", keywords,
                                     &fd, &sock, &o, &n, &head, &head_len,
                                     &tail, &tail_len, &flags))
        return NULL;

    off_t offset = o;
    size_t nbytes = n;
    off_t sent;
    int ret;
#ifdef __APPLE__
    sent = nbytes;
#endif

    if (head || tail) {
        struct iovec ivh = {head, head_len};
        struct iovec ivt = {tail, tail_len};
        struct sf_hdtr hdtr = {&ivh, 1, &ivt, 1};
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
    #if !defined(HAVE_LARGEFILE_SUPPORT)
        return Py_BuildValue("l", sent);
    #else
        return Py_BuildValue("L", sent);
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
    size_t count;
    char *hdr=NULL, *trail=NULL;
    int hdrsize, trailsize;
    ssize_t sts=0;
    struct sf_parms sf_iobuf;
    int rc;

    if (!PyArg_ParseTuple(args, "iiLk|s#s#",
                          &out_fd, &in_fd, &offset, &count, &hdr, &hdrsize,
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
    sf_iobuf.file_bytes = count;

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
    #if !defined(HAVE_LARGEFILE_SUPPORT)
        return Py_BuildValue("l", sts);
    #else
        return Py_BuildValue("L", sts);
    #endif
    }
}
/* --- end AIX --- */

/* --- start Linux --- */

#elif defined (__linux__)
#include <sys/sendfile.h>

static PyObject *
method_sendfile(PyObject *self, PyObject *args)
{
    int out_fd, in_fd;
    off_t offset;
    size_t count;
    ssize_t sent;

    if (!PyArg_ParseTuple(args, "iiLk", &out_fd, &in_fd, &offset, &count))
        return NULL;

    Py_BEGIN_ALLOW_THREADS;
    sent = sendfile(out_fd, in_fd, &offset, count);
    Py_END_ALLOW_THREADS;
    if (sent == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
#if !defined(HAVE_LARGEFILE_SUPPORT)
    return Py_BuildValue("l", sent);
#else
    return Py_BuildValue("L", sent);
#endif
}

#else /* --- end Linux --- */


static PyObject *
method_sendfile(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_NotImplementedError, "platform not supported");
    Py_INCREF(Py_None);
    return Py_None;
}
#endif



static PyMethodDef
SendfileMethods[] =
{
    {"sendfile",  method_sendfile,  METH_VARARGS | METH_KEYWORDS,
"sendfile(out, in, offset, nbytes)\n"
"sendfile(out, in, offset, nbytes, headers=None, trailers=None, flags=0)\n"
"\n"
"Copy *nbytes* bytes from file descriptor *in* to file descriptor *out*\n"
"starting from *offset* and return the number of bytes sent.\n"
"\n"
"The first function notation is supported by all platforms.\n"
"\n"
"On Linux, if *offset* is given as `None`, the bytes are read from the\n"
"current position of *in* and the position of *in* is updated. It returns\n"
"the same as above with offset being `None`.\n"
"\n"
"The second case may be used on Mac OS X and FreeBSD where *headers* and\n"
"*trailers* are arbitrary sequences of buffers that are written before and\n"
"after the data from *in* is written. It returns the same as the first case.\n"
"\n"
"On Mac OS X and FreeBSD, a value of 0 for *nbytes* specifies to send until\n"
"the end of *in* is reached.\n"
"\n"
"On Solaris, *out* may be the file descriptor of a regular file or the file\n"
"descriptor of a socket. On all other platforms, *out* must be the file\n"
"descriptor of an open socket.\n"
"\n"
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
static struct module_state _state;
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

