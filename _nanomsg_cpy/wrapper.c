#include <Python.h>
#include <structmember.h>
#include <bytesobject.h>
#include <nanomsg/nn.h>


#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

/* This might be a good idea or not */
#ifndef NO_CONCURRENY
#define CONCURRENCY_POINT_BEGIN Py_BEGIN_ALLOW_THREADS
#define CONCURRENCY_POINT_END Py_END_ALLOW_THREADS
#else
#define CONCURRENCY_POINT_BEGIN
#define CONCURRENCY_POINT_END
#endif


/* defined to allow the same source for 2.6+ and 3.2+ */
#ifdef IS_PY3K
#define Py_TPFLAGS_HAVE_CLASS     0L
#define Py_TPFLAGS_HAVE_NEWBUFFER 0L
#endif

const static char MODULE_NAME[] = "_nanomsg";


typedef struct {
    PyObject_HEAD
    void *msg;
    size_t size;
} Message;

static void
Message_dealloc(Message* self)
{
    if (self->msg != NULL) {
        nn_freemsg(self->msg);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
Message_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyErr_Format(PyExc_TypeError,
                 "cannot create '%.100s' instances us nn_alloc instead",
                 type->tp_name);
    return NULL;
}

static PyMemberDef Message_members[] = {
    {NULL}  /* Sentinel */
};

static PyMethodDef Message_methods[] = {
    {NULL}  /* Sentinel */
};

int Message_getbuffer(Message *self, Py_buffer *view, int flags) {
    if( self->msg == NULL)
    {
         PyErr_BadInternalCall();
         return -1;
    }
    return PyBuffer_FillInfo(view, (PyObject*)self, self->msg, self->size, 0, flags);
}

#ifndef IS_PY3K
static int Message_getreadbuffer(Message *self, int segment, void **ptrptr)
{
    if(segment != 0 || self->msg == NULL)
    {
         PyErr_BadInternalCall();
         return -1;
    }
    *ptrptr = ((Message*)self)->msg;
    return ((Message*)self)->size;
}

static int Message_getwritebuffer(Message *self, int segment, void **ptrptr)
{
    if(segment != 0 || self->msg == NULL)
    {
         PyErr_BadInternalCall();
         return -1;
    }
    *ptrptr = ((Message*)self)->msg;
    return ((Message*)self)->size;
}
#endif

static int Message_getsegcountproc(PyObject *self, int *lenp) {
    if (lenp != NULL) {
        *lenp = ((Message*)self)->size;
    }
    return 1;
}

static PyBufferProcs Message_bufferproces = {
#ifndef IS_PY3K
    (readbufferproc)Message_getreadbuffer,
    (writebufferproc)Message_getwritebuffer,
    (segcountproc)Message_getsegcountproc,
    NULL,
#endif
    (getbufferproc)Message_getbuffer,
    NULL
};

static PyObject *
Message_repr(Message * obj)
{
    return PyUnicode_FromFormat("<_nanomsg.Message size %zu, address %p >",
                               obj->size, obj->msg);
}


static PyObject *
Message_str(Message * obj)
{
    return PyBytes_FromStringAndSize(obj->msg, obj->size);
}

static PyTypeObject MessageType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_nanomsg.Message",          /*tp_name*/
    sizeof(Message),             /*tp_basicsize*/
    0,                           /*tp_itemsize*/
    (destructor)Message_dealloc, /*tp_dealloc*/
    0,                           /*tp_print*/
    0,                           /*tp_getattr*/
    0,                           /*tp_setattr*/
    0,                           /*tp_compare*/
    (reprfunc)Message_repr,      /*tp_repr*/
    0,                           /*tp_as_number*/
    0,                           /*tp_as_sequence*/
    0,                           /*tp_as_mapping*/
    0,                           /*tp_hash */
    0,                           /*tp_call*/
    (reprfunc)Message_str,       /*tp_str*/
    0,                           /*tp_getattro*/
    0,                           /*tp_setattro*/
    &Message_bufferproces,       /*tp_as_buffer*/
    Py_TPFLAGS_HAVE_CLASS | Py_TPFLAGS_HAVE_NEWBUFFER |
        Py_TPFLAGS_IS_ABSTRACT,  /*tp_flags*/
    "nanomsg allocated message wrapper supporting buffer protocol", /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    Message_methods,           /* tp_methods */
    Message_members,           /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    Message_new,               /* tp_new */
};

static PyObject *
_nanomsg_nn_errno(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", nn_errno());
}

static PyObject *
_nanomsg_nn_strerror(PyObject *self, PyObject *args)
{
    int error_number;
    if (!PyArg_ParseTuple(args, "i", &error_number))
        return NULL;
    return Py_BuildValue("s", nn_strerror(error_number));
}

static PyObject *
_nanomsg_nn_socket(PyObject *self, PyObject *args)
{
    int domain, protocol;
    if (!PyArg_ParseTuple(args, "ii", &domain, &protocol))
        return NULL;
    return Py_BuildValue("i", nn_socket(domain, protocol));
}

static PyObject *
_nanomsg_nn_close(PyObject *self, PyObject *args)
{
    int nn_result, socket;

    if (!PyArg_ParseTuple(args, "i", &socket))
        return NULL;

    CONCURRENCY_POINT_BEGIN
    nn_result = nn_close(socket);
    CONCURRENCY_POINT_END

    return Py_BuildValue("i", nn_result);
}

static const char _nanomsg_nn_setsockopt__doc__[] =
"set a socket option\n"
"\n"
"socket - socket number\n"
"level - option level\n"
"option - option\n"
"value - a readable byte buffer (not a Unicode string) containing the value\n"
"returns - 0 on success or < 0 on error\n\n";

static PyObject *
_nanomsg_nn_setsockopt(PyObject *self, PyObject *args)
{
    int nn_result, socket, level, option;
    Py_buffer value;

    if (!PyArg_ParseTuple(args, "iiis*", &socket, &level, &option, &value))
        return NULL;

    nn_result = nn_setsockopt(socket, level, option, value.buf, value.len);
    PyBuffer_Release(&value);
    return Py_BuildValue("i", nn_result);
}


static const char _nanomsg_nn_getsockopt__doc__[] =
"retrieve a socket option\n"
"\n"
"socket - socket number\n"
"level - option level\n"
"option - option\n"
"value - a writable byte buffer (e.g. a bytearray) which the option value "
"will be copied to.\n"
"returns - number of bytes copied or on error nunber < 0\n\n";

static PyObject *
_nanomsg_nn_getsockopt(PyObject *self, PyObject *args)
{
    int nn_result, socket, level, option;
    Py_buffer value;

    if (!PyArg_ParseTuple(args, "iiiw*", &socket, &level, &option, &value))
        return NULL;

    nn_result = nn_setsockopt(socket, level, option, value.buf, value.len);
    PyBuffer_Release(&value);
    return Py_BuildValue("i", nn_result);
}

static PyObject *
_nanomsg_nn_bind(PyObject *self, PyObject *args)
{
    int socket;
    const char *address;
    if (!PyArg_ParseTuple(args, "is", &socket, &address))
        return NULL;
    return Py_BuildValue("i", nn_bind(socket, address));
}

static PyObject *
_nanomsg_nn_connect(PyObject *self, PyObject *args)
{
    int socket;
    const char *address;
    if (!PyArg_ParseTuple(args, "is", &socket, &address))
        return NULL;
    return Py_BuildValue("i", nn_connect(socket, address));
}

static PyObject *
_nanomsg_nn_shutdown(PyObject *self, PyObject *args)
{
    int nn_result, socket, endpoint;

    if (!PyArg_ParseTuple(args, "ii", &socket, &endpoint))
        return NULL;

    CONCURRENCY_POINT_BEGIN
    nn_result = nn_shutdown(socket, endpoint);
    CONCURRENCY_POINT_END
    return Py_BuildValue("i", nn_result);
}

static PyObject *
_nanomsg_nn_send(PyObject *self, PyObject *args)
{
    int nn_result, socket, flags;
    Py_buffer buffer;

    if (!PyArg_ParseTuple(args, "is*i", &socket, &buffer, &flags))
        return NULL;

    CONCURRENCY_POINT_BEGIN
    nn_result = nn_send(socket, buffer.buf, buffer.len, flags);
    CONCURRENCY_POINT_END
    PyBuffer_Release(&buffer);
    return Py_BuildValue("i", nn_result);
}

static PyObject *
_nanomsg_nn_recv(PyObject *self, PyObject *args)
{
    int nn_result, socket, flags;
    Py_buffer buffer;
    Message* message;

    if(PyTuple_GET_SIZE(args) == 2) {
        if (!PyArg_ParseTuple(args, "ii", &socket, &flags))
            return NULL;
        message =  (Message*)PyType_GenericAlloc(&MessageType, 0);
        if(message == NULL) {
            return NULL;
        }
        CONCURRENCY_POINT_BEGIN
        nn_result = nn_recv(socket, &message->msg, NN_MSG, flags);
        CONCURRENCY_POINT_END
        if (nn_result < 0) {
            Py_DECREF((PyObject*)message);
            return Py_BuildValue("is", nn_result, NULL);
        }
        message->size = nn_result;
        return Py_BuildValue("iN", nn_result, message);
    }
    else {
        if(!PyArg_ParseTuple(args, "iw*i", &socket, &buffer, &flags))
            return NULL;
        CONCURRENCY_POINT_BEGIN
        nn_result = nn_recv(socket, buffer.buf, buffer.len, flags);
        CONCURRENCY_POINT_END
        PyBuffer_Release(&buffer);
        return Py_BuildValue("iO", nn_result, PyTuple_GET_ITEM(args, 1));
    }
}

static PyObject *
_nanomsg_nn_device(PyObject *self, PyObject *args)
{
    int socket_1, socket_2;
    if (!PyArg_ParseTuple(args, "ii", &socket_1, &socket_2))
        return NULL;
    return Py_BuildValue("i", nn_device(socket_1, socket_2));
}

static PyObject *
_nanomsg_nn_term(PyObject *self, PyObject *args)
{
    nn_term();
    Py_RETURN_NONE;
}


static PyObject *
_nanomsg_nn_allocmsg(PyObject *self, PyObject *args)
{
    size_t size;
    int type;
    Message *message;

    if (!PyArg_ParseTuple(args, "ni", &size, &type))
        return NULL;

    message = (Message*)PyType_GenericAlloc(&MessageType, 0);

    message->msg = nn_allocmsg(size, type);

    if (message->msg == NULL) {
        Py_DECREF((PyObject*)message);
        Py_RETURN_NONE;
    }

    message->size = size;

    return (PyObject*)message;
}

static PyMethodDef module_methods[] = {
    {"nn_errno", _nanomsg_nn_errno, METH_VARARGS, "retrieve the current errno"},
    {"nn_strerror", _nanomsg_nn_strerror, METH_VARARGS, "convert an error number into human-readable string"},
    {"nn_socket", _nanomsg_nn_socket, METH_VARARGS, "create an SP socket"},
    {"nn_close", _nanomsg_nn_close, METH_VARARGS, "close an SP socket"},
    {"nn_setsockopt", _nanomsg_nn_setsockopt, METH_VARARGS, _nanomsg_nn_setsockopt__doc__},
    {"nn_getsockopt", _nanomsg_nn_getsockopt, METH_VARARGS, _nanomsg_nn_getsockopt__doc__},
    {"nn_bind", _nanomsg_nn_bind, METH_VARARGS, "add a local endpoint to the socket"},
    {"nn_connect", _nanomsg_nn_connect, METH_VARARGS, "add a remote endpoint to the socket"},
    {"nn_shutdown", _nanomsg_nn_shutdown, METH_VARARGS, "remove an endpoint from a socket"},
    {"nn_send", _nanomsg_nn_send, METH_VARARGS, "send a message"},
    {"nn_recv", _nanomsg_nn_recv, METH_VARARGS, "receive a message"},
    {"nn_device", _nanomsg_nn_device, METH_VARARGS, "start a device"},
    {"nn_term", _nanomsg_nn_term, METH_VARARGS, "notify all sockets about process termination"},
    {"nn_allocmsg", _nanomsg_nn_allocmsg, METH_VARARGS, "allocate a message"},
    {NULL, NULL, 0, NULL}
};


#ifndef IS_PY3K
PyMODINIT_FUNC
init_nanomsg(void)
{
    PyObject* m;

    if (PyType_Ready(&MessageType) < 0)
        return;
    m = Py_InitModule(MODULE_NAME, module_methods);
    if (m == NULL)
      return;

    Py_INCREF(&MessageType);
    PyModule_AddObject(m, "Message", (PyObject *)&MessageType);
}
#else
static struct PyModuleDef _nanomsg_module = {
   PyModuleDef_HEAD_INIT,
   MODULE_NAME,
   NULL,
   -1,
   module_methods
};

PyMODINIT_FUNC
PyInit__nanomsg(void)
{
    PyObject* m;

    if (PyType_Ready(&MessageType) < 0)
        return NULL;
    m = PyModule_Create(&_nanomsg_module);
    if(m != NULL) {
        PyModule_AddObject(m, "Message", (PyObject *)&MessageType);
    }
    return m;
}
#endif
