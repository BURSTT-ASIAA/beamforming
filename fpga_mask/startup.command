sudo ./build/integral --iova-mode='va' --no-pci --no-telemetry -l 4-12,20-28

sudo dpdk-hugepages.py -p 2M -r 1G
sudo mkdir /dev/hugepages-2M
sudo mount -t hugetlbfs -o pagesize=2M none /dev/hugepages-2M

sudo numactl --membind=0 truncate -s 1073741824 /dev/hugepages/fpga0.bin
sudo numactl --membind=0 truncate -s 1073741824 /dev/hugepages/fpga1.bin
sudo numactl --membind=0 truncate -s 1073741824 /dev/hugepages/fpga2.bin
sudo numactl --membind=0 truncate -s 1073741824 /dev/hugepages/fpga3.bin
sudo numactl --membind=0 truncate -s 2147483648 /dev/hugepages/voltage.bin

xxd -l 16 -s 1048576000 /dev/hugepages/fpga0.bin
xxd -l 16 -s 2048000000 /dev/hugepages/voltage.bin

# (8192+64)*2*400*1000*30
xxd -l 256 -s 198144000000 /mnt/fpga0
