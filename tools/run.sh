#!/usr/bin/env bash

set -e

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." &> /dev/null && pwd)

BUILD_DIR="$PROJECT_DIR/build"

ENABLE_DEBUG=0
ENABLE_KVM=0
MEMORY="4G"
CPUS=4
QEMU_ARCH=x86_64
QEMU_IMAGE="$PROJECT_DIR/amethyst.iso"

function error() {
    BRED="\033[1;31m"
    RST="\033[0m"
    printf "%s: ${BRED}error:${RST} %s\n" "$0" "$1"
    exit 1
}

function show_help() {
    echo "$0: Script for running the Amethyst OS within QEMU"
    echo
    echo "Usage: $0 [OPTION]... [VAR=VALUE]..."
    echo "-a <arch>, --arch=<arch>          | Set the target architecture [$QEMU_ARCH]"
    echo "-c <ncpu>, --arch=<ncpu>          | Set the number of processors [$CPUS]"
    echo "-d, --debug                       | Enable debugging via gdb"
    echo "-I <image>, --image=<image>       | Set the image file to be loaded [$QEMU_IMAGE]"
    echo "-K, --kvm                         | Enable KVM accelleration"
    echo "-m <memory>, --memory=<memory>    | Amount of allocated memory for the virtual machine [$MEMORY]"
    echo
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -a)
            shift
            QEMU_ARCH=$1
            ;;
        --arch=*)
            QEMU_ARCH=${1#*=}
            ;;
        -K | --kvm)
            ENABLE_KVM=1
            ;;
        -c)
            shift
            CPUS=$1
            ;;
        --cpus=*)
            CPUS=${1#*=}
            ;;
        -m)
            shift
            MEMORY=$1
            ;;
        --memory=*)
            MEMORY=${1#*=}
            ;;
        -d | --debug)
            ENABLE_DEBUG=1
            ;;
        -I)
            shift
            QEMU_IMAGE=$1
            ;;
        --image=*)
            QEMU_IMAGE=${1#*=}
            ;;
        *)
            error "unknown option -- '$1'"
            ;;
    esac

    shift
done

SYMBOL_FILE="$BUILD_DIR/amethyst-0.0.1-${QEMU_ARCH}.sym"

QEMU="qemu-system-${QEMU_ARCH}"
QEMUFLAGS+="-m ${MEMORY} -serial stdio -smp cpus=${CPUS} -no-reboot -no-shutdown -cdrom ${QEMU_IMAGE} -boot order=d"

[ $ENABLE_KVM -eq 1 ] && QEMUFLAGS+=" -enable-kvm"

if [ ! -e "${QEMU_IMAGE}" ]; then
    error "`${QEMU_IMAGE}`: No such file or directory. You need to bootstrap the system first."
    exit 1
fi

if [ $ENABLE_DEBUG -eq 1 ]; then
    $QEMU $QEMUFLAGS -s -S &
    gdb -ex "target remote localhost:1234" -ex "symbol-file ${SYMBOL_FILE}"
else
    $QEMU $QEMUFLAGS
fi
