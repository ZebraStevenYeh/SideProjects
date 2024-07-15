import os
import mmap
import hexdump
import argparse
import colorama
import sys
import copy
from colorama import Fore, Style

def init_arg_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "-S", dest = 'AddrString', 
                        type = str,
                        default = 0,
                        help = 'PCI address string')
    parser.add_argument("-h", "-help", dest = 'CallHelp',
                        type = bool,
                        default = 0,
                        help = 'Show the usage of this app.')


if __name__ == "__main__":
    args = init_arg_parser()
    print(args)