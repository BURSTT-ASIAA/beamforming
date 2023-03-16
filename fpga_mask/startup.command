sudo ./build/integral --iova-mode='va' --no-pci --no-telemetry -l 5-15,18-31,37-47,50-63

mount -t hugetlbfs -o pagesize=2M none /dev/hugepages-2M

sudo truncate -s 4294967296 /dev/hugepages/beams.bin
sudo truncate -s 2147483648 /dev/hugepages/voltage.bin

