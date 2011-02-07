/*
 * $Id$
 */

/* py-sendfile

   A Python module interface to sendfile(2)
   Copyright (C) 2005 Ben Woolley <user tautolog at gmail>

   The AIX support code is:

   Copyright (C) 2008,2009 Niklas Edmundsson <nikke@acc.umu.se>

   Currently maintained by Giampaolo Rodola'
   Copyright (C) 2011 <g.rodola@gmail.com>

   This is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <Python.h>
#include <stdlib.h>


/* --- begin FreeBSD / Dragonfly --- */
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

int
PyParse_off_t(PyObject* arg, void* addr)
{
#if !defined(HAVE_LARGEFILE_SUPPORT)
    *((off_t*)addr) = PyLong_AsLong(arg);
#else
    *((off_t*)addr) = PyLong_Check(arg) ? PyLong_AsLongLong(arg)
 : PyLong_AsLong(arg);
#endif
    if (PyErr_Occurred())
        return 0;
    return 1;
}


static int
iov_setup(struct iovec **iov, Py_buffer **buf, PyObject *seq, int cnt, int type)
{
    int i, j;
    *iov = PyMem_New(struct iovec, cnt);
    if (*iov == NULL) {
        PyErr_NoMemory();
        return 0;
    }
    *buf = PyMem_New(Py_buffer, cnt);
    if (*buf == NULL) {
        PyMem_Del(*iov);
        PyErr_NoMemory();
        return 0;
    }

    for (i = 0; i < cnt; i++) {
        if (PyObject_GetBuffer(PySequence_GetItem(seq, i), &(*buf)[i],
                type) == -1) {
            PyMem_Del(*iov);
            for (j = 0; j < i; j++) {
                PyBuffer_Release(&(*buf)[j]);
           }
            PyMem_Del(*buf);
            return 0;
        }
        (*iov)[i].iov_base = (*buf)[i].buf;
        (*iov)[i].iov_len = (*buf)[i].len;
    }
    return 1;
}

static void
iov_cleanup(struct iovec *iov, Py_buffer *buf, int cnt)
{
    int i;
    PyMem_Del(iov);
    for (i = 0; i < cnt; i++) {
        PyBuffer_Release(&buf[i]);
    }
    PyMem_Del(buf);
}

static PyObject *
method_sendfile(PyObject *self, PyObject *args, PyObject *kwdict)
{
    int in, out;
    Py_ssize_t ret;
    Py_ssize_t len;
    off_t offset;
    PyObject *headers = NULL, *trailers = NULL;
    Py_buffer *hbuf, *tbuf;
    off_t sbytes;
    struct sf_hdtr sf;
    int flags = 0;
    sf.headers = NULL;
    sf.trailers = NULL;

    static char *keywords[] = {"out", "in", "offset", "count",
                               "headers", "trailers", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwdict, "iiO&n|OOi:sendfile",
        keywords, &out, &in, PyParse_off_t, &offset, &len,
        &headers, &trailers, &flags)) {
            return NULL;
    }

    if (headers != NULL) {
        if (!PySequence_Check(headers)) {
            PyErr_SetString(PyExc_TypeError,
                "sendfile() headers must be a sequence or None");
            return NULL;
        } else {
            sf.hdr_cnt = PySequence_Size(headers);
            if (sf.hdr_cnt > 0 && !iov_setup(&(sf.headers), &hbuf,
                    headers, sf.hdr_cnt, PyBUF_SIMPLE))
                return NULL;
        }
    }
    if (trailers != NULL) {
        if (!PySequence_Check(trailers)) {
            PyErr_SetString(PyExc_TypeError,
                "sendfile() trailers must be a sequence or None");
            return NULL;
        } else {
            sf.trl_cnt = PySequence_Size(trailers);
            if (sf.trl_cnt > 0 && !iov_setup(&(sf.trailers), &tbuf,
                    trailers, sf.trl_cnt, PyBUF_SIMPLE))
                return NULL;
        }
    }

    Py_BEGIN_ALLOW_THREADS
    ret = sendfile(in, out, offset, len, &sf, &sbytes, flags);
    Py_END_ALLOW_THREADS

    if (sf.headers != NULL)
        iov_cleanup(sf.headers, hbuf, sf.hdr_cnt);
    if (sf.trailers != NULL)
         iov_cleanup(sf.trailers, tbuf, sf.trl_cnt);

    if (ret < 0) {
        if ((errno == EAGAIN) || (errno == EBUSY)) {
            if (sbytes != 0) {
                goto done;
            }
            else {
                // upper application is supposed to retry
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
        return Py_BuildValue("ll", sbytes, offset + sbytes);
    #else
        return Py_BuildValue("LL", sbytes, offset + sbytes);
    #endif
}
/* --- end FreeBSD / Dragonfly --- */

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
    } else {
        return Py_BuildValue("kL", sts, offset);
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
    ssize_t sts;

    if (!PyArg_ParseTuple(args, "iiLk", &out_fd, &in_fd, &offset, &count))
        return NULL;

    Py_BEGIN_ALLOW_THREADS;
    sts = sendfile(out_fd, in_fd, &offset, count);
    Py_END_ALLOW_THREADS;
    if (sts == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
#if !defined(HAVE_LARGEFILE_SUPPORT)
    return Py_BuildValue("lL", sts, offset);
#else
    return Py_BuildValue("LL", sts, offset);
#endif
}

#endif  /* --- end Linux --- */


static PyMethodDef
SendfileMethods[] =
{
    {"sendfile",  method_sendfile,  METH_VARARGS | METH_KEYWORDS,
"sendfile(out_fd, in_fd, offset, count) = [position, sent]\n"
"\n"
"FreeBSD only:\n"
"sendfile(out_fd, in_fd, offset, count, headers_and_or_trailers) = [position, sent]\n"
"\n"
"AIX:\n"
"sendfile(out_fd, in_fd, offset, count, header, trailer) = [position, sent]\n"
"where header and trailer is optional\n"
"\n"
"Direct interface to FreeBSD and Linux sendfile(2) using the Linux API, except that errors are turned into Python exceptions, and instead of returning only the amount of data being sent, it returns a tuple that contains the new file pointer, and the amount of data that has been sent.\n"
"\n"
"For example:\n"
"\n"
"from sendfile import *\n"
"sendfile(out_socket.fileno(), in_file.fileno(), int_start, int_len)\n"
"\n"
"On Linux, item 0 of the return value is always a reliable file pointer. The value specified in the offset argument is handed to the syscall, which then updates it according to how much data has been sent. The length of data sent is returned in item 1 of the return value.\n"
"\n"
"FreeBSD sf_hdtr is also supported as an additional argument which can be a string, list, or tuple. If it is a string, it will create a struct iovec of length 1 containing the string which will be sent as the header. If a list, it will create a struct iovec of the length of the list, containing the strings in the list, which will be concatenated by the syscall to form the total header. If a tuple, it will expect a string or list of strings in two items: the first representing the header, and the second representing the trailer, processed the same way as the header. You can send only a footer by making the header an empty string or list, or list of empty strings.\n"
"\n"
"On AIX, the returned position is the actual file position as updated by the kernel. The returned sent bytes are including header and trailer, so if you are doing non-blocking IO you have to take that into account.\n"
"FreeBSD example with string header:\n"
"\n"
"from sendfile import *\n"
"sendfile(out_socket.fileno(), in_file.fileno(), 0, 0, \"HTTP/1.1 200 OK\\r\\nContent-Type: text/html\\r\\nConnection: close\\r\\n\\r\\n\")\n"
"\n"
"FreeBSD example with both header and trailer as a string:\n"
"\n"
"from sendfile import *\n"
"sendfile(out_socket.fileno(), in_file.fileno(), int_start, int_len, ('BEGIN', 'END'))\n"
"\n"
"FreeBSD example with mixed types:\n"
"\n"
"from sendfile import *\n"
"sendfile(out_socket.fileno(), in_file.fileno(), int_start, int_len, ([magic, metadata_len, metadata, data_len], md5))\n"
"\n"
"Although the FreeBSD sendfile(2) requires the socket file descriptor to be specified as the second argument, this function will ALWAYS require the socket as the first argument, like Linux and Solaris. Also, if an sf_hdtr is specified, the function will return the total data sent including all of the headers and trailers. Note that item 0 of the return value, the file pointer position, is determined on FreeBSD only by adding offset and count, so if not all of the data has been sent, this value will be wrong. You will have to use the value in item 1, which tells you how much total data has actually been sent, and be aware that header and trailer data are included in that value, so you may need to reconstruct the headers and/or trailers yourself if you would like to find out exactly which data has been sent. However, if you do not send any headers or trailers, you can just add item 1 to where you started to find out where you need to start from again. I do not consider this much of a problem because if you are sending header and trailer data, the protocol will likely not allow you to just keep sending from where the failure occured without getting into complexities, anyway.\n"
    },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

struct module_state {
    PyObject *error;
};


static int
ins(PyObject *module, char *symbol, long value)
{
    return PyModule_AddIntConstant(module, symbol, value);
}


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
    struct module_state *st = GETSTATE(module);
#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}

