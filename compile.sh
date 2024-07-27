#!/bin/bash

KERNEL=kernel8

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image modules dtbs -j6

