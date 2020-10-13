from libc.stdlib cimport malloc, free
from libcpp.string cimport string
from libcpp.map cimport map
from libcpp.utility cimport pair
from libcpp cimport bool
from libcpp.vector cimport vector
from libcpp.functional cimport function
import numpy as np

cdef extern from "src/headers/globals.h":
    cdef cppclass Array[T]:
        cppclass iterator:
            T operator*()
            iterator operator++()
            bool operator==(iterator)
            bool operator!=(iterator)
        iterator begin()
        iterator end()
        int getSize()


cdef extern from "src/headers/dl85.h":
    string search ( int* supports,
                    int ntransactions,
                    int nattributes,
                    int nclasses,
                    int *data,
                    int *target,
                    float maxError,
                    bool stopAfterError,
                    bool iterative,
                    PyErrorWrapper tids_error_class_callback,
                    PyFastErrorWrapper supports_error_class_callback,
                    PyPredictorErrorWrapper tids_error_callback,
                    PyExampleWeightWrapper example_weight_callback,
                    # PyTestErrorWrapper predict_error_callback,
                    bool tids_error_class_is_null,
                    bool supports_error_class_is_null,
                    bool tids_error_is_null,
                    bool example_weight_is_null,
                    bool predict_error_is_null,
                    int maxdepth,
                    int minsup,
                    int max_estimators,
                    bool infoGain,
                    bool infoAsc,
                    bool repeatSort,
                    int timeLimit,
                    map[int, pair[int, int]]* continuousMap,
                    bool save,
                    bool verbose_param) except +

cdef extern from "src/headers/py_error_function_wrapper.h":
    cdef cppclass PyErrorWrapper:
        PyErrorWrapper()
        PyErrorWrapper(object) # define a constructor that takes a Python object
             # note - doesn't match c++ signature - that's fine!

cdef extern from "src/headers/py_fast_error_function_wrapper.h":
    cdef cppclass PyFastErrorWrapper:
        PyFastErrorWrapper()
        PyFastErrorWrapper(object) # define a constructor that takes a Python object
             # note - doesn't match c++ signature - that's fine!

cdef extern from "src/headers/py_predictor_error_function_wrapper.h":
    cdef cppclass PyPredictorErrorWrapper:
        PyPredictorErrorWrapper()
        PyPredictorErrorWrapper(object) # define a constructor that takes a Python object
             # note - doesn't match c++ signature - that's fine!

cdef extern from "src/headers/py_example_weight_function_wrapper.h":
    cdef cppclass PyExampleWeightWrapper:
        PyExampleWeightWrapper()
        PyExampleWeightWrapper(object) # define a constructor that takes a Python object
             # note - doesn't match c++ signature - that's fine!

# cdef extern from "src/headers/py_test_error_function_wrapper.h":
    # cdef cppclass PyTestErrorWrapper:
        # PyTestErrorWrapper()
        # PyTestErrorWrapper(object) # define a constructor that takes a Python object
             # note - doesn't match c++ signature - that's fine!



def solve(data,
          target,
          func=None,
          fast_func=None,
          predictor_func=None,
          weight_func=None,
          # test_func=None,
          max_depth=1,
          min_sup=1,
          max_estimators=1,
          max_error=0,
          stop_after_better=False,
          iterative=False,
          time_limit=0,
          verb=False,
          desc=False,
          asc=False,
          repeat_sort=False,
          continuousMap=None,
          bin_save=False,
          predictor=False):

    cdef PyErrorWrapper f_user = PyErrorWrapper(func)
    error_null_flag = True
    if func is not None:
        error_null_flag = False

    cdef PyFastErrorWrapper f_user_fast = PyFastErrorWrapper(fast_func)
    fast_error_null_flag = True
    if fast_func is not None:
        fast_error_null_flag = False

    cdef PyPredictorErrorWrapper f_user_predictor = PyPredictorErrorWrapper(predictor_func)

    cdef PyExampleWeightWrapper f_example_weight = PyExampleWeightWrapper(weight_func)
    weight_null_flag = False

    # cdef PyTestErrorWrapper f_test_weight = PyTestErrorWrapper(test_func)
    # test_null_flag = False

    if max_estimators <= 1:
            # test_func = None
            # test_null_flag = True
            weight_func = None
            weight_null_flag = True




    data = data.astype('int32')


    ntransactions, nattributes = data.shape
    classes, supports = np.unique(target, return_counts=True)
    nclasses = len(classes)
    supports = supports.astype('int32')


    data = data.transpose()
    # print(data)
    if np.array_equal(data, data.astype('bool')) is False:  # WARNING: maybe categorical (not binary) inputs will be supported in the future
        raise ValueError("Bad input type. DL8.5 actually only supports binary (0/1) inputs")
    if not data.flags['C_CONTIGUOUS']:
        data = np.ascontiguousarray(data) # Makes a contiguous copy of the numpy array.
    cdef int [:, ::1] data_view = data
    cdef int *data_matrix = &data_view[0][0]

    cdef int [::1] target_view
    cdef int *target_array = NULL
    if target is not None:
        target = target.astype('int32')
        if not target.flags['C_CONTIGUOUS']:
            target = np.ascontiguousarray(target) # Makes a contiguous copy of the numpy array.
        target_view = target
        target_array = &target_view[0]
    else:
        nclasses = 0


    if not supports.flags['C_CONTIGUOUS']:
        supports = np.ascontiguousarray(supports) # Makes a contiguous copy of the numpy array.
    cdef int [::1] supports_view = supports

    # max_err = max_error - 1  # because maxError but not be reached
    if max_error < 0:  # raise error when incompatibility between max_error value and stop_after_better value
        stop_after_better = False

    cont_map = NULL
    if continuousMap is not None:
        #cont_map must be defined properly
        cont_map = NULL

    info_gain = not (desc == False and asc == False)

    pred = not predictor

    out = search(&supports_view[0],
                 ntransactions,
                 nattributes,
                 nclasses,
                 data_matrix,
                 target_array,
                 max_error,
                 stop_after_better,
                 iterative,
                 tids_error_class_callback = f_user,
                 supports_error_class_callback = f_user_fast,
                 tids_error_callback = f_user_predictor,
                 example_weight_callback = f_example_weight,
                 predict_error_callback = f_test_weight,
                 tids_error_class_is_null = error_null_flag,
                 supports_error_class_is_null = fast_error_null_flag,
                 tids_error_is_null = pred,
                 example_weight_is_null = weight_null_flag,
                 predict_error_is_null = test_null_flag,
                 maxdepth = max_depth,
                 minsup = min_sup,
                 max_estimators = max_estimators,
                 infoGain = info_gain,
                 infoAsc = asc,
                 repeatSort = repeat_sort,
                 timeLimit = time_limit,
                 continuousMap = NULL,
                 save = bin_save,
                 verbose_param = verb)

    return out.decode("utf-8")