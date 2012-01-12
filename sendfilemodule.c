/* py-sendfile 1.0
   A Python module interface to sendfile(2)
   Copyright (C) 2005 Ben Woolley <user ben at host tautology.org>

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
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

static PyObject *
method_sendfile(PyObject *self, PyObject *args)
{
    int fd, s, sts;
    off_t offset, sbytes;
    size_t nbytes;
    PyObject *headers;
    struct iovec *header_iovs;
    struct iovec *footer_iovs;
    struct sf_hdtr hdtr;

    headers = NULL;

    if (!PyArg_ParseTuple(args, "iiLi|O:sendfile", &fd, &s, &offset, &nbytes, &headers))
        return NULL;

    if (headers != NULL) {
        int i, listlen;
        PyObject *string;
        char *buf;
        int headerlist_len;
        int headerlist_string = 0;
        int footerlist_len;
        int footerlist_string = 0;
        PyObject *headerlist;
        PyObject *footerlist;

        if (PyTuple_Check(headers) && PyTuple_Size(headers) > 1) {
            //printf("arg is tuple length %d\n", PyTuple_Size(headers));
            headerlist = PyTuple_GetItem(headers, 0);
            if (PyList_Check(headerlist)) {
                headerlist_len = PyList_Size(headerlist);
            } else if (PyString_Check(headerlist)) {
                headerlist_string = 1;
                headerlist_len = 1;
            } else {
                headerlist_len = 0;
            }

            footerlist = PyTuple_GetItem(headers, 1);
            if (PyList_Check(footerlist)) {
                //printf("footer is list\n");
                footerlist_len = PyList_Size(footerlist);
            } else if (PyString_Check(footerlist)) {
                //printf("footer is string\n");
                footerlist_string = 1;
                footerlist_len = 1;
            } else {
                //printf("footer is invalid\n");
                footerlist_len = 0;
            }
        } else {
            if (PyTuple_Check(headers)) {
                headerlist = PyTuple_GetItem(headers, 0);
            } else {
                headerlist = headers;
            }
            if (PyList_Check(headerlist)) {
            	headerlist_len = PyList_Size(headerlist);
            } else if (PyString_Check(headerlist)) {
                headerlist_string = 1;
                headerlist_len = 1;
            } else {
                headerlist_len = 0;
            }

            footerlist_len = 0;
            footer_iovs = 0;
        }
        //printf("header list size and type: %d, %d\nfooter list size and type: %d, %d\n", headerlist_len, headerlist_string, footerlist_len, footerlist_string);

        header_iovs = (struct iovec*) malloc( sizeof(struct iovec) * headerlist_len );
        for (i=0; i < headerlist_len; i++) {
            if (headerlist_string) {
                //printf("found string\n");
                string = headerlist;
            } else {
                //printf("found list\n");
                string = PyList_GET_ITEM(headerlist, i);                
            }
            buf = (char *) PyString_AsString(string);                
            //printf("buf: %s\n", buf);
            header_iovs[i].iov_base = buf;
            header_iovs[i].iov_len = PyString_GET_SIZE(string);
        }

        footer_iovs = (struct iovec*) malloc( sizeof(struct iovec) * footerlist_len );
        for (i=0; i < footerlist_len; i++) {
            if (footerlist_string) {
                //printf("found string\n");
                string = footerlist;
            } else {
                //printf("found list\n");
                string = PyList_GET_ITEM(footerlist, i);                
            }
            buf = (char *) PyString_AsString(string);
            //printf("buf: %s\n", buf);
            footer_iovs[i].iov_base = buf;
            footer_iovs[i].iov_len = PyString_GET_SIZE(string);
        }

        hdtr.headers = header_iovs;
        hdtr.hdr_cnt = headerlist_len;
        hdtr.trailers = footer_iovs;
        hdtr.trl_cnt = footerlist_len;

	Py_BEGIN_ALLOW_THREADS;
        sts = sendfile(s, fd, (off_t) offset, (size_t) nbytes, &hdtr, (off_t *) &sbytes, 0);
	Py_END_ALLOW_THREADS;
        free(header_iovs);
        free(footer_iovs);
    } else {
	Py_BEGIN_ALLOW_THREADS;
        sts = sendfile(s, fd, (off_t) offset, (size_t) nbytes, NULL, (off_t *) &sbytes, 0);
	Py_END_ALLOW_THREADS;
    }
    if (sts == -1) {
        if (errno == EAGAIN || errno == EINTR) {
            return Py_BuildValue("LL", offset + nbytes, sbytes);
        } else {
            PyErr_SetFromErrno(PyExc_OSError);
            return NULL;
        }
    } else {
        return Py_BuildValue("LL", offset + nbytes, sbytes);
    }
}

#else
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
    sts = sendfile(out_fd, in_fd, (off_t *) &offset, (ssize_t) count);
    Py_END_ALLOW_THREADS;
    if (sts == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    } else {
        return Py_BuildValue("Lk", offset, sts);
    }
}

#endif

static PyMethodDef SendfileMethods[] = {
    {"sendfile",  method_sendfile, METH_VARARGS,
"sendfile(out_fd, in_fd, offset, count) = [position, sent]\n"
"\n"
"FreeBSD only:\n"
"sendfile(out_fd, in_fd, offset, count, headers_and_or_trailers) = [position, sent]\n"
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
"\n"
"The variable has_sf_hdtr is provided for determining whether sf_hdtr is supported."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static void
insint (PyObject *d, char *name, int value)
{
    PyObject *v = PyInt_FromLong((long) value);
    if (!v || PyDict_SetItemString(d, name, v))
        PyErr_Clear();

    Py_XDECREF(v);
}

PyMODINIT_FUNC
initsendfile(void)
{
    PyObject *m = Py_InitModule("sendfile", SendfileMethods);

    PyObject *d = PyModule_GetDict (m);

#if defined(__FreeBSD__) || defined(__DragonFly__)
    insint (d, "has_sf_hdtr", 1);
#else
    insint (d, "has_sf_hdtr", 0);
#endif
    PyModule_AddStringConstant(m, "__doc__", "Direct interface to FreeBSD and Linux sendfile(2), for sending file data to a socket directly via the kernel.");
    PyModule_AddStringConstant(m, "__version__", "1.2.2");
}
