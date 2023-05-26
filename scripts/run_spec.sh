#!/usr/bin/zsh
gem5path=/mnt/disk/liuhao/gem5/build/ARM/gem5.opt
configpath=/mnt/disk/liuhao/gem5/configs/example/se.py

# example
gccpath=/mnt/disk/liuhao/spec_run/403.gcc/run_base_ref_arm64.0001/gcc_base.arm64
# single
# $gem5path $configpath --cpu-type=DerivO3CPU --caches --l2cache -I 100000000 -c $gccpath -i "/mnt/disk/liuhao/spec_run/403.gcc/run_base_ref_arm64.0001/166.i"
# smt
$gem5path $configpath --cpu-type=DerivO3CPU --caches --l2cache -I 2400000000 --smt -c "$gccpath;$gccpath" -i "/mnt/disk/liuhao/spec_run/403.gcc/run_base_ref_arm64.0001/166.i;/mnt/disk/liuhao/spec_run/403.gcc/run_base_ref_arm64.0001/166.i"
