## ❄️ Build Instruction's For NixOS
> [!IMPORTANT]
> **Requirements:** [Nix](https://nixos.org/download) with flakes enabled.

### Clone the Repo
> to any directory u prefer for ex. `$HOME/projects/fadesktop`

### Obtain your API credentials

You will need **api_id** and **api_hash** to access the Telegram API servers.
> To obtain them **[click here](https://my.telegram.org/apps)**.

### Building

**Install using `flake.nix` (no cloning needed, u can skip obatiaing API credentials if you choose this method):**
  ```nix
  {
  inputs = {
    fagram.url = "github:fagramdesktop/fadesktop";
    # ...
  };
  
    # ...
  }
  ```

**Or build from source (please change API credentials when using this method):** <br>
  go to `nix/unwrapped.nix` and change `TDESKTOP_API_ID` and `TDESKTOP_API_HASH` to your's first
 
  ```
  git clone --recurse-submodules https://github.com/fagramdesktop/fadesktop
  cd fadesktop
  nix run nixpkgs#nix-prefetch-github -- fagramdesktop fadesktop --rev dev --fetch- submodules
  git add .
  nix build .#fagram-desktop
  ./result/bin/fagram
  ```

> [!IMPORTANT]
> after running `nix-prefetch-github` make sure to update the the `hash`, `rev` in unwrapped.nix file

That's it enjoy!
