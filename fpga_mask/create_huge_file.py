import sys
import numpy as np

fn = sys.argv[1]
pages = int(sys.argv[2])
GB = 2**30

print(fn, pages)
data = np.memmap(fn, dtype='u1', mode='r+', shape=(pages * GB))

for n in range(pages):
    offset = n * GB
    print(data[offset:offset+16])
