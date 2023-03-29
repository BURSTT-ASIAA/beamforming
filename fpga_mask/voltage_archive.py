import numpy as np
from ctypes import *
import time

NBUF = 5
BLK_SIZE = 1024*400*1000
DATA_SIZE = BLK_SIZE * NBUF
ARCH_BLKS = 60

voltage = np.memmap('/dev/hugepages/voltage.bin', dtype='byte', shape=(DATA_SIZE))
blk_ptr = cast(c_void_p(voltage.ctypes.data + DATA_SIZE), POINTER(c_long))

last_blk = blk_ptr.contents.value
filename = None
nblk = 0

while True:
    while last_blk == blk_ptr.contents.value:
        time.sleep(0.01)

    if filename is None:
        filename = f'/sdisk1/VoltageData/Voltage_{int(time.time())}'
        fp = open(filename, 'wb')

    data = voltage[BLK_SIZE*last_blk:BLK_SIZE*(last_blk+1)]
    data.tofile(fp)

    last_blk = (last_blk + 1) % NBUF
    nblk += 1
    print(f'Block #{last_blk} written, count: {nblk}')
    if nblk >= ARCH_BLKS:
        nblk = 0
        fp.close()
        filename = None

