#!/usr/bin/bash

#sudo dpdk-hugepages.py -p 2M -r 1G
#sudo mkdir /dev/bonsai
#sudo mount -t hugetlbfs -o pagesize=2M none /dev/bonsai

create_file() {
    sudo truncate -s $(($2*$4)) $1
    sudo chmod 666 $1
    sudo numactl -m $3 python3 create_huge_file.py $1 $2 $4
}
create_file /mnt/ens2f0np0/fpga.bin 124 0 1073741824
create_file /dev/hugepages/fpga0.bin 1 0 1073741824
#create_file /dev/hugepages/fpga1.bin 1 0 1073741824
#create_file /dev/hugepages/fpga2.bin 1 1 1073741824
#create_file /dev/hugepages/fpga3.bin 1 1 1073741824
create_file /dev/hugepages/voltage.bin 2 0 1073741824
create_file /mnt/bonsai/matrix 1 0 2097152

sudo ./mqueue
sudo ln -s /mnt/ens2f0np0/fpga.bin /mnt/fpga0
#sudo chmod 666 /mnt/ens2f0np0/fpga.bin
sudo chmod 666 `readlink -f /mnt/fpga0`
sudo chmod 666 /dev/mqueue/burstt

## (8192+64)*2*400*1000*30
# xxd -l 256 -s 198144000000 /mnt/fpga0
## (8192+64)*2*400*1000*20
# xxd -l 256 -s 132096000000 /mnt/fpga0

# sudo ./build/integral --iova-mode='va' --no-pci --no-telemetry -l 4-12,20-28
# python3 intensity_server.py
