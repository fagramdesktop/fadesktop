{
  description = "FAgram Desktop";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forEachSystem = nixpkgs.lib.genAttrs systems;
    in {
      packages = forEachSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };
          fagram-desktop = pkgs.kdePackages.callPackage ./nix/default.nix {
            tg_owt = pkgs.telegram-desktop.tg_owt;
          };
        in {
          inherit fagram-desktop;
          default = fagram-desktop;
        }
      );
    };
}
