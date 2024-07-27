#!/bin/bash

#필요 패키지 설치
sudo apt install bc bison flex libssl-dev make libc6-dev libncurses5-dev

#크로스툴체인 설치
sudo apt install crossbuild-essential-arm64


KERNEL=kernel8

#config생성
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig

#config 파일로 빌드
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image modules dtbs -j6

#sd카드 삽입

#sd카드안의 root에 module 설치
sudo env PATH=$PATH make -j12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=./media/asdcv0428/rootfs/lib/modules modules_install

#빌드된 arch/arm64/boot 하단의 dtb 파일 옮기고 .img 파일도 이름 바꿔서 복사
sudo cp mnt/boot/$KERNEL.img mnt/boot/$KERNEL-backup.img
sudo cp arch/arm64/boot/Image mnt/boot/$KERNEL.img
sudo cp arch/arm64/boot/dts/broadcom/*.dtb ./media/asdcv0428/bootfs/
sudo cp arch/arm64/boot/dts/overlays/*.dtb* ./media/asdcv0428/bootfs/overlays/
sudo cp arch/arm64/boot/dts/overlays/README ./media/asdcv0428/bootfs/overlays/

#확인 변경 완료
