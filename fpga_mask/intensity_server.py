import numpy as np
from ctypes import *
import socket
#import errno
import time

NBUF = 16
BLK_SIZE = 1024*16*1000
DATA_SIZE = BLK_SIZE * NBUF
HOST = '0.0.0.0'
PORT = 5000
FPGA_BLK_SZ = (8192 + 64) * 800000
FPGA_BLK_NB = 30
FPGA_SIZE = 2**30*185
FPGA_DATA_SIZE = FPGA_BLK_SZ * FPGA_BLK_NB
MASK_BLK_SZ = 100000

intensity = np.memmap('/dev/hugepages/fpga0.bin', dtype='float32', shape=(DATA_SIZE))
blk_ptr = cast(c_void_p(intensity.ctypes.data + DATA_SIZE * 4), POINTER(c_long))
clk_counter = np.zeros(8, dtype='u1')

fpga_data = np.memmap('/mnt/fpga0', dtype='u1', shape=(FPGA_SIZE))

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

            hdr_id = blk_ptr[last_blk + 1]
            p = FPGA_BLK_SZ * hdr_id
            data = intensity[BLK_SIZE*last_blk:BLK_SIZE*(last_blk+1)]
            try:
                conn.sendall(fpga_data[p:p+64])
                conn.sendall(data)
            except socket.error as e:
#                if e.errno != errno.ECONNRESET:
#                    raise
                print('Connection closed for', addr)
                break

            print(f'Block #{last_blk} sent, header id= {hdr_id}, counter={fpga_data[p:p+5]}')
            last_blk = (last_blk + 1) % NBUF
