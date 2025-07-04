{ pkgs ? import <nixpkgs> {} }:
let
    cross = import <nixpkgs> {
        crossSystem = { config = "x86_64-elf"; };
    };
in
cross.mkShell.override {
    stdenv = pkgs.gcc14Stdenv;
} {
    buildInputs = with cross.buildPackages; [
        gcc14
        binutils
        gdb
        curl
    ];

    nativeBuildInputs = with pkgs; [
        gcc14
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
    ];
}
