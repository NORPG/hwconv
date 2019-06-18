#include <Python.h>
#include <numpy/arrayobject.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int my_gpio_fd;

static PyObject *SpamError;

static PyObject *hwconv_getpixel (PyObject * self, PyObject * args);
static PyObject *hwconv_system (PyObject * self, PyObject * args);
static PyObject *hwconv_pyrDown (PyObject * self, PyObject * args);

static void do_conv (void *data_in, void *data_out);

static PyMethodDef SpamMethods[] = {
  {"system", hwconv_system, METH_VARARGS,
   "Execute a shell command."},
  {"getpixel", hwconv_getpixel, METH_VARARGS,
   "get a pixel in numpy array"},
  {"pyrDown", hwconv_pyrDown, METH_VARARGS,
   "get a pixel in numpy array"},
  {NULL, NULL, 0, NULL}		/* Sentinel */
};

static PyObject *
hwconv_system (PyObject * self, PyObject * args)
{
  const char *command;
  int sts;

  if (!PyArg_ParseTuple (args, "s", &command))
    return NULL;
  sts = system (command);
  return Py_BuildValue ("i", sts);
}

static PyObject *
hwconv_getpixel (PyObject * self, PyObject * args)
{
  PyArrayObject *dtype;
  uint32_t i = 0, j = 0, k = 0;

  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &dtype))
    return NULL;

  if (dtype == NULL)
    return NULL;

  void *ptr;

  ptr = PyArray_GetPtr (dtype, (npy_intp[])
			{
			i});
  printf ("%p\n\r", ptr);

  ptr = PyArray_GetPtr (dtype, (npy_intp[])
			{
			i, j});
  printf ("%p\n\r", ptr);

/*  for (i = 0; i < 512; i++)
    {
      printf ("-----%3d-----\n\r", i);
      for (j = 0; j < 512; j++)
	{
//	  for (k = 0; k < 3; k++)
//	    {
	      ptr = PyArray_GetPtr (dtype, (npy_intp[])
{				    
				    i, j}
	      );
	      printf ("%p\n\r ", ptr);
//	    }
	}
    }
*/

  ptr = PyArray_DATA (dtype);
  printf ("%p\n\r", ptr);

  printf ("C A\n\r");
  printf ("%d %d \n\r", PyArray_IS_C_CONTIGUOUS (dtype),
	  PyArray_ISALIGNED (dtype));

  npy_intp *DIMS = PyArray_DIMS (dtype);
  for (i = 0; i < 2; i++)
    printf ("%d ", DIMS[i]);

  printf ("\n\r");

  return Py_BuildValue ("i", 1);
}

static PyObject *
hwconv_pyrDown (PyObject * self, PyObject * args)
{
  PyArrayObject *dtype;

  PyObject *rtv;
  PyObject *ret;
  npy_intp i = 0, j = 0, k = 0;
  npy_intp a;
  uint8_t *ptr;

  uint8_t data_in[64];
  uint8_t data_out[16];

  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &dtype))
    return NULL;

  if (dtype == NULL)
    return NULL;

//  printf ("get PyArray\n\r");

  npy_intp *DIMS = PyArray_DIMS (dtype);

  int NDIM = PyArray_NDIM (dtype);

  if ((DIMS[0] % 4) != 0)
    return NULL;

  if ((DIMS[1] % 4) != 0)
    return NULL;

//  printf ("get PyArray's dims\n\r");

  npy_intp *DIMS_R[2];

  for (i = 0; i < NDIM; i++)
    DIMS_R[i] = (DIMS[i] - 4) / 2;

  rtv = PyArray_Zeros (NDIM, DIMS_R, dtype->descr, 0);


//  printf ("alloc return value\n\r");

  for (i = 0; i < (DIMS[0] - 4); i = i + 4)
    {
      for (j = 0; j < (DIMS[1] - 4); j = j + 4)
	{
	  for (a = 0; a < 8; a++)
	    {
	      npy_intp col = i + a;
	      ptr = (uint8_t *) PyArray_GetPtr (dtype, (npy_intp[])
						{
						col, j}
	      );
	      memcpy (data_in + (a * 8), (void *) ptr, 8);
//            printf ("data_in %d, %d, %d ", a, col, j);
	    }
//        printf ("\n\r");

	  do_conv (data_in, data_out);

	  for (a = 0; a < 2; a++)
	    {
	      npy_intp col = (i >> 1) + a;
	      npy_intp row = (j >> 1);

	      ptr = (uint8_t *) PyArray_GetPtr (rtv, (npy_intp[])
						{
						col, row}
	      );
	      *ptr = *(data_out + (a * 8));
//            printf ("data_out %d, %d, %d ", a, col, row);
	      ptr++;
	      *ptr = *(data_out + (a * 8) + 2);

//            printf ("data_out %d, %d, %d ", a, col, row + 1);
	    }
//        printf ("\n\r");
	}
    }

//  printf ("cal finish\n\r");

  ret = Py_BuildValue ("O", rtv);

//  printf ("build return value\n\r");

  Py_INCREF (rtv);

//  printf ("rtv decref\n\r");
  return ret;
}

PyMODINIT_FUNC
inithwconv (void)
{
  PyObject *m;

  m = Py_InitModule ("hwconv", SpamMethods);
  if (m == NULL)
    return;

  SpamError = PyErr_NewException ("hwconv.error", NULL, NULL);
  Py_INCREF (SpamError);
  PyModule_AddObject (m, "error", SpamError);

  /*      open mygpio device      */
  my_gpio_fd = open ("/dev/mygpio", O_RDWR);

  /*      check return value      */
  if (my_gpio_fd < 0)
    {
      printf ("[ERROR] can't open mygpio\n\r");
      return;
    };

  import_array ();
}

int
main (int argc, char *argv[])
{
  /* Pass argv[0] to the Python interpreter */
  Py_SetProgramName (argv[0]);

  /* Initialize the Python interpreter.  Required. */
  Py_Initialize ();

  /* Add a static module */
  inithwconv ();

  return 0;
}

static void
do_conv (void *data_in, void *data_out)
{
  int index, value;
  uint32_t *pi_di = (uint32_t *) data_in;
  uint32_t *pi_do = (uint32_t *) data_out;
  value = (uint32_t) 1 << 1;
  write (my_gpio_fd, &value, (size_t) 0);
  for (index = 0; index < 16; index++)
    {
      write (my_gpio_fd, pi_di, index + 16);
      pi_di++;
    }

  value = ((uint32_t) 1 << 1) | ((uint32_t) 1 << 0);
  write (my_gpio_fd, &value, (size_t) 0);

  value = 0;
  while (value == 0)
    read (my_gpio_fd, &value, (size_t) 1);

  for (index = 0; index < 4; index++)
    {
      read (my_gpio_fd, pi_do, index + 4);
      pi_do++;
    };

  value = ((uint32_t) 1 << 1);
  write (my_gpio_fd, &value, 0);
}
