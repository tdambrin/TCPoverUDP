#!/usr/bin/env python3
import sys
import filecmp


if __name__ == '__main__':

    if filecmp.cmp(sys.argv[1], sys.argv[2]):
        print("These files are the same.")
    else:
        print("These files are different.")