import numpy as np
from ctypes import *
import socket
#import errno
import time

NBUF = 16
BLK_SIZE = 1024*16*1000
DATA_SIZE = BLK_SIZE * NBUF
HOST = '0.0.0.0'
PORT = 5001

intensity = np.memmap('/dev/hugepages/fpga0.bin', dtype='float32', shape=(DATA_SIZE))
blk_ptr = cast(c_void_p(intensity.ctypes.data + DATA_SIZE * 4), POINTER(c_long))

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen(1)

    while True:
        conn, addr = s.accept()
        print('Connected by', addr)

        last_blk = blk_ptr.contents.value
        while True:
            while last_blk == blk_ptr.contents.value:
                time.sleep(0.01)

            data = intensity[BLK_SIZE*last_blk:BLK_SIZE*(last_blk+1)]
            try:
                conn.sendall(data)
            except socket.error as e:
#                if e.errno != errno.ECONNRESET:
#                    raise
                print('Connection closed for', addr)
                break

            print(f'Block #{last_blk} sent')
            last_blk = (last_blk + 1) % NBUF
