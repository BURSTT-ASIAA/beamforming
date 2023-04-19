import sys
import numpy as np

fn = sys.argv[1]
pages = int(sys.argv[2])
size = int(sys.argv[3])

print(fn, pages, size)
data = np.memmap(fn, dtype='u1', mode='r+', shape=(pages * size))

for n in range(pages):
    offset = n * size
    print(data[offset:offset+16])
