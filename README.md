<div align="center">

<img alt="Telegram" width="150x" src="https://github.com/FajoX1/fagramdesktop/blob/dev/Telegram/Resources/art/icon512@2x.png?raw=true">

<h1><a href='https://fagram.app'>FAgram Desktop</a></h1>

#### This is the complete source code of FAgram Desktop.

</div>

> ***✏️ Note: This is a maintained fork of FAgram Desktop. The original project by [FajoX1](https://github.com/FajoX1/fagramdesktop) has been discontinued. This fork exists to keep the project up-to-date with upstream Telegram Desktop changes.***

> ***✏️ Source code is published under GPLv3 with OpenSSL exception, the license is available [here][license].***

<a href="https://t.me/FAgramClient"><img alt="Telegram" src="https://img.shields.io/badge/Telegram_Channel-0a0a0a?style=for-the-badge&logo=telegram"></a>
<a href="https://t.me/FAgram_Group"><img alt="Telegram" src="https://img.shields.io/badge/Telegram_Chat-0a0a0a?style=for-the-badge&logo=telegram"></a>
<a href="https://github.com/FajoX1/fagramdesktop"><img alt="Original Repository" src="https://img.shields.io/badge/Original_Repo-0a0a0a?style=for-the-badge&logo=github"></a>

<div align="center">

![[Preview of FAgram Desktop]][preview_image]

</div>

## <h2><img src="https://github.com/hikariatama/assets/raw/master/680-it-developer-flat.webp" height="50" align="middle"> Supported systems</h2>

The latest version is available for

- Windows (x64)
    - [Portable/ZIP](https://github.com/burhancodes/fagramdesktop/releases)
    - Via [Scoop](https://scoop.sh/) (powershell)
        ```powershell
        scoop bucket add fagram-scoop https://github.com/burhancodes/fagram-scoop
        scoop install fagram
        ```

- Linux
    - [Arch/AUR](https://aur.archlinux.org/packages/fagram-bin)
    - [Fedora/RPM](https://copr.fedorainfracloud.org/coprs/burhanverse/fagram/)
    - [Ubuntu/DEB](https://github.com/burhancodes/fagram-deb)

For detailed insturctions on how to install on linux [Click Here](https://burhanverse.eu.org/?article=fagram-desktop)

## <h2 border="none"><img src="https://github.com/hikariatama/assets/raw/master/981-consultation-flat.webp" height="54" align="middle">Translation</h2>

> You can help to translate FAgram.

* <h3><b><a href="https://crowdin.com/project/fagram">Crowdin</a></b></h3>

## <h2><img src="https://github.com/hikariatama/assets/raw/master/1312-micro-sd-card-flat.webp" height="50" align="middle"> Third-party</h2>

* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html)) and Qt 5.15 ([LGPL](http://doc.qt.io/qt-5/lgpl.html)) slightly patched
* OpenSSL 3.2.1 ([Apache License 2.0](https://www.openssl.org/source/apache-license-2.0.txt))
* WebRTC ([New BSD License](https://github.com/desktop-app/tg_owt/blob/master/LICENSE))
* zlib ([zlib License](http://www.zlib.net/zlib_license.html))
* LZMA SDK 9.20 ([public domain](http://www.7-zip.org/sdk.html))
* liblzma ([public domain](http://tukaani.org/xz/))
* Google Breakpad ([License](https://chromium.googlesource.com/breakpad/breakpad/+/master/LICENSE))
* Google Crashpad ([Apache License 2.0](https://chromium.googlesource.com/crashpad/crashpad/+/master/LICENSE))
* GYP ([BSD License](https://github.com/bnoordhuis/gyp/blob/master/LICENSE))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))
* OpenAL Soft ([LGPL](https://github.com/kcat/openal-soft/blob/master/COPYING))
* Opus codec ([BSD License](http://www.opus-codec.org/license/))
* FFmpeg ([LGPL](https://www.ffmpeg.org/legal.html))
* Guideline Support Library ([MIT License](https://github.com/Microsoft/GSL/blob/master/LICENSE))
* Range-v3 ([Boost License](https://github.com/ericniebler/range-v3/blob/master/LICENSE.txt))
* Open Sans font ([Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0.html))
* Vazirmatn font ([SIL Open Font License 1.1](https://github.com/rastikerdar/vazirmatn/blob/master/OFL.txt))
* Emoji alpha codes ([MIT License](https://github.com/emojione/emojione/blob/master/extras/alpha-codes/LICENSE.md))
* xxHash ([BSD License](https://github.com/Cyan4973/xxHash/blob/dev/LICENSE))
* QR Code generator ([MIT License](https://github.com/nayuki/QR-Code-generator#license))
* CMake ([New BSD License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
* Hunspell ([LGPL](https://github.com/hunspell/hunspell/blob/master/COPYING.LESSER))
* Ada ([Apache License 2.0](https://github.com/ada-url/ada/blob/main/LICENSE-APACHE))

## <h2><img src="https://github.com/hikariatama/assets/raw/master/1326-command-window-line-flat.webp" height="50" align="middle"> Build instructions</h2>

* Windows [32-bit](docs/building-win.md) [(64-bit)](docs/building-win-x64.md)
* [macOS](docs/building-mac.md)
* [GNU/Linux using Docker][linux_build]

## ❤️ Credits

* [FajoX1 (Original FAgram Developer)](https://github.com/FajoX1)
* [Telegram Desktop](https://github.com/telegramdesktop/tdesktop)
* [rabbitGram](https://github.com/rabbitGramDesktop)
* [Ayugram](https://github.com/ayugram/AyugramDesktop)
* [materialgram](https://github.com/kukuruzka165/materialgram)
* [64gram](https://github.com/TDesktop-x64/tdesktop)
* [Kotatogram](https://github.com/kotatogram/kotatogram-desktop)
* [Design480 (for solar icons)](https://www.figma.com/@480design)
* [hikariatama (for emojis in readme)](https://github.com/hikariatama)

[//]: # (LINKS)
[fagram]: https://t.me/FAgramClient
[fagram_desktop]: https://t.me/FAgramClient
[telegram_desktop]: https://github.com/telegramdesktop/tdesktop
[telegram_api]: https://core.telegram.org
[telegram_proto]: https://core.telegram.org/mtproto
[license]: LICENSE
[win]: https://t.me/FAgramWindows
[linux]: https://t.me/FAgramLinux
[win32_build]: docs/building-win.md
[win64_build]: docs/building-win64.md
[mac_build]: docs/building-mac.md
[linux_build]: docs/building-linux.md
[preview_image]: https://raw.githubusercontent.com/fajox1/fagramdesktop/dev/docs/assets/preview.png "Preview of FAgram Deskop"