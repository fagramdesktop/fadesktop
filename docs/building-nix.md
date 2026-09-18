## ❄️ Nix & NixOS Instructions

FAgram Desktop's official Nix expressions and Flake are maintained in a dedicated repository:
👉 **[github:fagramdesktop/nix](https://github.com/fagramdesktop/nix)**

> [!IMPORTANT]
> **Requirements:** [Nix](https://nixos.org/download) with flakes enabled (`experimental-features = nix-command flakes`).

---

### Quick Run

You can run FAgram Desktop directly without installing:

```bash
nix run github:fagramdesktop/nix

# Explicitly choose prebuilt binary:
nix run github:fagramdesktop/nix#prebuilt

# Or compile and run from source:
nix run github:fagramdesktop/nix#source
```

Install to your user profile:

```bash
# Fast prebuilt binary:
nix profile install github:fagramdesktop/nix#prebuilt

# Or build from source:
nix profile install github:fagramdesktop/nix#source
```

---

### Flake Integration (NixOS / Home Manager)

Add `fagram` to your flake inputs:

```nix
{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    fagram.url = "github:fagramdesktop/nix";
  };

  outputs = { self, nixpkgs, fagram, ... }: {
    nixosConfigurations.yourhostname = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ({ pkgs, ... }: {
          environment.systemPackages = [
            fagram.packages.${pkgs.system}.default
          ];
        })
      ];
    };
  };
}
```

---

### Building from Source

To build locally from the Nix packaging repo:

```bash
git clone https://github.com/fagramdesktop/nix.git
cd nix
nix build .#fagram-desktop
./result/bin/fagram
```

For custom API credentials or local overrides, edit `pkgs/unwrapped.nix` in your clone of `fagramdesktop/nix`.
