{ pkgs ? import <nixpkgs> {} }:
let
    cross = import <nixpkgs> {
        crossSystem = { config = "x86_64-elf"; };
    };

    qemu-trace = pkgs.qemu.overrideAttrs (old: {
        configureFlags = (old.configureFlags or []) ++ [
            "--target-list=x86_64-softmmu"
            "--enable-trace-backends=log"
        ];
    });
in
cross.mkShell.override {
    stdenv = pkgs.gcc14Stdenv;
} {
    buildInputs = with cross.buildPackages; [
        gcc14
        binutils
        gdb
        curl
        bear
        qemu-trace
    ];

    nativeBuildInputs = with pkgs; [
        llvmPackages.clang-tools
        gcc14
        gf
        binutils
        pkg-config
        autobuild
        automake
        autoconf
        gnumake
        nasm
        openssl
        libarchive
        libtool
        xorriso
        libedit
        libgit2
        python314
    ];

    shellHook = ''
        export TMPDIR="$(mktemp -d /tmp/nix-shell-XXXXXX)"
    '';
}
