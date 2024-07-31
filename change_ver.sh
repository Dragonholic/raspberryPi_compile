#!/bin/bash



KERNEL=kernel8
   93  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
   94  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image modules dtbs
   95  sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image modules dtbs -j6
   96  sudo env PATH=$PATH make -j12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=mnt/root modules_install
   
   97  sudo cp 6.6.42-v8+/ /media/asdcv0428/rootfs/lib/modules/
   98  sudo -r cp 6.6.42-v8+/ /media/asdcv0428/rootfs/lib/modules/
   99  sudo cp -r 6.6.42-v8+/ /media/asdcv0428/rootfs/lib/modules/

#확인 변경 완료
