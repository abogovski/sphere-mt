import numpy as np
import os
from subprocess import Popen, PIPE
from timeit import default_timer as timer

TYPE = np.int64
MAX_VALUE = 2 ** 31
MIN_VALUE = - 2 ** 31
TEMP_SHUFFLED = "temp_shuffle.bin"
TEMP_SORTED = "temp_sorted.bin"
PATH_TO_EXT_SORT = "03-sort"


def print_work_time(func):
    def wrapper(*args, **kwargs):
        start = timer()
        result = func(*args, **kwargs)
        end = timer()
        print("Execution take {}".format(end - start))
        return result
    return wrapper


def make_test(array_size,
              min_value=MIN_VALUE,
              max_value=MAX_VALUE,
              test_file_shuffle=TEMP_SHUFFLED):
    arr = np.random.randint(min_value, max_value, size=array_size, dtype=TYPE)
    arr.tofile(test_file_shuffle)


@print_work_time
def call_extern_sort(test_file_shuffle=TEMP_SHUFFLED,
                     test_file_sorted=TEMP_SORTED,
                     path_to_ext_sort=PATH_TO_EXT_SORT):
    proc = Popen(
        "./{} {} {}".format(path_to_ext_sort,
                            test_file_shuffle,
                            test_file_sorted),
        shell=True,
        stdout=PIPE,
        stderr=PIPE
    )
    proc.wait()
    proc.communicate()


def check(array_size,
          min_value=MIN_VALUE,
          max_value=MAX_VALUE,
          test_file_shuffle=TEMP_SHUFFLED,
          test_file_sorted=TEMP_SORTED,
          path_to_ext_sort=PATH_TO_EXT_SORT):

    make_test(array_size, min_value, max_value, test_file_shuffle)
    call_extern_sort(test_file_shuffle, test_file_sorted, path_to_ext_sort)
    arr = np.fromfile(test_file_sorted, dtype=TYPE)
    os.remove(test_file_shuffle)
    os.remove(test_file_sorted)
    return (arr[1:] < arr[:-1]).sum() == 0


for size in (511, 512, 513, 3*512-1, 3*512, 3*512+1, 10*512-1, 10*512, 10*512+1):
    print("Testing on {} size".format(size))
    if check(size):
        print("Pass")
    else:
        print("Failed")
        break
