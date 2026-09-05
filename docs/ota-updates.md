# OTA Updates

Self-update system for Fagram Desktop via a GitHub repository.

The client periodically checks a JSON manifest at the configured URL. If a newer version is available, it downloads a signed update package and installs it.

The merged build requests the **`current6`** feed (lib_base `AutoUpdateVersion()` returned `4` in 2.5.x and was bumped to `6` by the v7.2.5 merge). The **`current4`** feed is still published alongside for older clients that have not yet migrated; both files have the same shape and are kept in sync.

---

## How it works

1. The client requests `{UPDATE_URL}/current6` — a JSON file with per-platform versions. Older clients on the v4 feed request `{UPDATE_URL}/current4`; the OTA repo carries both.
2. If a newer version exists, it downloads the update file (e.g. `tx64upd6005002`) from `{UPDATE_URL}/{link}`.
3. The file is RSA-signed (1024-bit); the client verifies the signature before installing.
4. After verification the update is decompressed (LZMA) and applied via `Updater` (Updater.exe on Windows).

`UPDATE_URL` is set in `Telegram/SourceFiles/storage/localstorage.cpp` in `readAutoupdatePrefixRaw()`.

---

## Setup for your fork

### Files to modify

| File | What to change |
|------|---------------|
| `storage/localstorage.cpp` | URL of your updates repo in `readAutoupdatePrefixRaw()` |
| `config.h` | `UpdatesPublicKey` / `UpdatesPublicBetaKey` — your RSA public keys |
| `_other/packer.cpp` | `PublicKey` / `PublicBetaKey` — same public keys (duplicate for Packer) |
| `DesktopPrivate/packer_private.h` | `PrivateKey` / `PrivateBetaKey` — your RSA private keys |
| `DesktopPrivate/alpha_private.h` | `AlphaPrivateKey` — private key for alpha versions |
| `mtproto/mtp_instance.cpp` | Commented out `autoupdate_url_prefix` from Telegram API |
| `core/update_checker.cpp` | Disabled MTP checker (updates via Telegram channels) |

### RSA key generation

Two pairs (stable + beta) and one for alpha are needed. Key size must be **1024 bit** (Packer format compatibility).

```python
# pip install cryptography
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization

key = rsa.generate_private_key(public_exponent=65537, key_size=1024)

# Public key → config.h, packer.cpp
pub = key.public_key().public_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PublicFormat.PKCS1
).decode()

# Private key → DesktopPrivate/packer_private.h
priv = key.private_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PrivateFormat.TraditionalOpenSSL,
    encryption_algorithm=serialization.NoEncryption()
).decode()
```

Generate separately for stable (`PrivateKey`), beta (`PrivateBetaKey`) and alpha (`AlphaPrivateKey`).

**Warning: Private keys must never be committed to a public repository!**
**Warning: Changing keys will break updates for all installed clients — a manual reinstall will be required.**

### Directory structure

```
<your-workspace>/
├── fagramdesktop/          # main repository (tdesktop fork)
├── ota/                    # updates repository
├── DesktopPrivate/         # private keys (DO NOT publish!)
│   ├── packer_private.h    # PrivateKey + PrivateBetaKey
│   └── alpha_private.h     # AlphaPrivateKey
└── Libraries/              # build dependencies
```

The path to `DesktopPrivate` is hardcoded via `#include` (4 levels up from `_other/packer.cpp`).

---

## Manifest format (`current6` and `current4`)

```json
{
  "win64": {
    "stable": {
      "released": "6005002",
      "link": "/tx64upd6005002"
    },
    "beta": {
      "released": "6005003",
      "link": "/tx64upd6005003"
    }
  },
  "linux": {
    "stable": {
      "released": "6005002",
      "link": "/tlinuxupd6005002"
    }
  }
}
```

- `released` — integer version: `major × 1000000 + minor × 1000 + patch`. Example: `6.5.2` → `6005002`.
- `link` — path to the update file (starts with `/`), concatenated with `UPDATE_URL`.

Platform keys are defined in `Platform::AutoUpdateKey()`:

| Platform    | Key     | File prefix  | Example            |
|-------------|---------|--------------|---------------------|
| Windows x64 | `win64` | `tx64upd`    | `tx64upd6005002`    |
| Linux       | `linux` | `tlinuxupd`  | `tlinuxupd6005002`  |

---

## Building and publishing an update

### 1. Bump the version

Edit `Telegram/build/version`:

```
AppVersion         6005002
AppVersionStrMajor 6.5
AppVersionStrSmall 6.5.2
AppVersionStr      6.5.2
BetaChannel        0
AlphaVersion       0
AppVersionOriginal 6.5.2
```

### 2. Build Release

#### Windows (x64)

From **x64 Native Tools Command Prompt for VS 2022**:

```bat
cd <your-repo>\Telegram
call configure.bat
cd ..\out
cmake --build . --config Release --target Telegram
```

#### Linux

```bash
cd <your-repo>
cmake --build out --config Release --target Telegram
```

### 3. Create the update package

From `out/Release`:

```bat
:: Windows x64
Packer.exe -version 6005002 -path Telegram.exe -path Updater.exe -path "modules\x64\d3d\d3dcompiler_47.dll" -target win64
```

```bash
# Linux
./Packer -version 6005002 -path Telegram -path Updater -target linux
```

Packer signs the package with the private key from `DesktopPrivate/packer_private.h` and creates a file like `tx64upd6005002` or `tlinuxupd6005002`.

### 4. Publish

#### Via publish_update.py

```bash
python Telegram/build/publish_update.py \
  --version 6005002 \
  --platform win64 \
  --file out/Release/tx64upd6005002 \
  --updates-repo path/to/ota

# For beta channel add --beta
# To commit without pushing add --no-push
```

The script copies the file to the updates repo, updates both `current6` and `current4`, commits and pushes.

#### Manually

1. Copy the update file to the ota repo.
2. Create/update `current6` and `current4` (format above) with matching contents.
3. `git add`, `git commit`, `git push`.

### 5. Verify

After pushing, the manifest and update file will be available at your repo's URL.
Clients check for updates every 8–16 hours. Manual check: **Settings → Advanced → click on version**.

---

## Hosting: GitHub raw vs. Releases

By default `raw.githubusercontent.com` is used — files are stored directly in the repository.

**Limitations:**
- GitHub limits files to 100 MB. Updates are usually ~40–80 MB.
- If the file exceeds 100 MB, use Git LFS: `git lfs track "t*upd*"`.
- Alternative — GitHub Releases, but you'll need to change the URL scheme and make `link` in the manifest an absolute URL.

---

## Updates repository structure

```
ota/
├── current6              # JSON manifest, the v6 feed (current merged clients)
├── current4              # JSON manifest, the v4 feed (legacy clients)
├── tx64upd6005002        # Windows x64
├── tlinuxupd6005002      # Linux
└── README.md
```

Both manifests are kept byte-for-byte identical so a single release publish covers every client.

Old update files can be deleted — they're only needed while clients on the previous version still exist.
