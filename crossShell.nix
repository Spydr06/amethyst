let pkgs = import <nixpkgs> {
    overlays = [
        (self: super: { gcc = self.gcc13; })
    ];
    crossSystem = {
        config = "x86_64-elf";  
    };
};
in pkgs.callPackage (
    {mkShell, openssl, xorriso}:
    mkShell {
        nativeBuildInputs = [
            openssl
            xorriso
        ];
    }
) {}
