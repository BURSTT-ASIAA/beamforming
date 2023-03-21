sudo ./build/integral --iova-mode='va' --no-pci --no-telemetry -l 5-15,18-31,37-47,50-63

sudo dpdk-hugepages.py -p 2M -r 1G
sudo mkdir /dev/hugepages-2M
sudo mount -t hugetlbfs -o pagesize=2M none /dev/hugepages-2M

sudo truncate -s 4294967296 /dev/hugepages/beams.bin
sudo truncate -s 1073741824 /dev/hugepages/fpga0.bin
sudo truncate -s 1073741824 /dev/hugepages/fpga1.bin
sudo truncate -s 1073741824 /dev/hugepages/fpga2.bin
sudo truncate -s 1073741824 /dev/hugepages/fpga3.bin
sudo truncate -s 2147483648 /dev/hugepages/voltage.bin

xxd -l 16 -s 1048576000 /dev/hugepages/fpga0.bin
xxd -l 16 -s 2048000000 /dev/hugepages/voltage.bin
