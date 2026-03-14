#!/usr/bin/env bash

set -e

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." &> /dev/null && pwd)

BUILD_DIR="$PROJECT_DIR/build"

ENABLE_DEBUG=0
ENABLE_KVM=0
ENABLE_SDL=0
ENABLE_X11=0
ENABLE_GL=1

TRACE_FILE=/dev/stderr

MEMORY="4G"
CPUS=4
QEMU_ARCH=x86_64
QEMU_IMAGE="$PROJECT_DIR/amethyst.iso"
QEMU_NVME=
GDB=gdb

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
    echo "-c <ncpu>, --cpus=<ncpu>          | Set the number of processors [$CPUS]"
    echo "-d, --debug                       | Enable debugging via gdb"
    echo "-I <image>, --image=<image>       | Set the image file to be loaded [$QEMU_IMAGE]"
    echo "-K, --kvm                         | Enable KVM accelleration"
    echo "-S, --sdl                         | Enable SDL"
    echo "    --x11                         | Enable X11"
    echo "    --gl                          | Disable OpenGL"
    echo "    --nvme=<image>                | Use <image> as an NVMe device"
    echo "    --nvme-trace                  | Enable NVMe tracing"
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
        -S | --sdl)
            ENABLE_SDL=1
            ;;
        --x11)
            ENABLE_X11=1
            ;;
        --gl)
            ENABLE_GL=0
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
        --nvme=*)
            QEMU_NVME=${1#*=}
            ;;
        --nvme-trace)
            QEMUFLAGS+="-trace pci_nvme_*"
            ;;
        --gdb=*)
            GDB=${1#*=}
            ;;
        --help)
            show_help
            exit 0
            ;;
        -h)
            show_help
            exit 0
            ;;
        *)
            error "unknown option -- '$1'"
            ;;
    esac

    shift
done

SYMBOL_FILE="$BUILD_DIR/amethyst-0.0.1-${QEMU_ARCH}.sym"

QEMU="qemu-system-${QEMU_ARCH}"
QEMUFLAGS+=" -m ${MEMORY} -serial stdio \
    -smp cpus=${CPUS} -no-reboot -no-shutdown \
    -drive file=${QEMU_IMAGE},media=cdrom \
    -boot order=d \
    -trace file=${TRACE_FILE}"

[ $ENABLE_KVM -eq 1 ] && QEMUFLAGS+=" -enable-kvm"

if [ ! -z "${QEMU_NVME}" ]; then
    QEMUFLAGS+=" -drive file=${QEMU_NVME},if=none,id=nvm,format=raw \
        -device nvme,serial=deadbeef,drive=nvm"
fi

if [ ! -e "${QEMU_IMAGE}" ]; then
    error "`${QEMU_IMAGE}`: No such file or directory. You need to bootstrap the system first."
    exit 1
fi

if [ $ENABLE_SDL -eq 1 ]; then
    QEMUFLAGS+=" -display sdl"
    export SDL_RENDER_SCALE_QUALITY=0
else
    QEMUFLAGS+=" -display gtk"
fi

[ $ENABLE_GL -eq 1 ] && QEMUFLAGS+=",gl=on"

[ $ENABLE_X11 -eq 1 ] && export GDK_BACKEND=x11

if [ $ENABLE_DEBUG -eq 1 ]; then
    $QEMU $QEMUFLAGS -s -S &
    $GDB -ex "target remote localhost:1234" -ex "symbol-file ${SYMBOL_FILE}"
else
    $QEMU $QEMUFLAGS
    # GDK_BACKEND=x11 GDK_SCALE=1 GDK_DPI_SCALE=1 $QEMU $QEMUFLAGS -display gtk,zoom-to-fit=off,gl=on
    # SDL_RENDER_SCALE_QUALITY=0 $QEMU $QEMUFLAGS -display sdl,gl=on
fi

