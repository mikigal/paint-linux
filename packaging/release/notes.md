## Installing

| System | Download | Install |
|---|---|---|
| Arch Linux, CachyOS | `paint-for-linux-@VERSION@-1-x86_64.pkg.tar.zst` | `sudo pacman -U paint-for-linux-@VERSION@-1-x86_64.pkg.tar.zst` |
| Debian 13 | `paint-for-linux_@VERSION@-1_debian13_amd64.deb` | `sudo apt install ./paint-for-linux_@VERSION@-1_debian13_amd64.deb` |
| Ubuntu 26.04 and newer | `paint-for-linux_@VERSION@-1_ubuntu26.04_amd64.deb` | `sudo apt install ./paint-for-linux_@VERSION@-1_ubuntu26.04_amd64.deb` |
| Fedora 44 | `paint-for-linux-@VERSION@-1.fedora44.x86_64.rpm` | `sudo dnf install ./paint-for-linux-@VERSION@-1.fedora44.x86_64.rpm` |
| openSUSE Tumbleweed | `paint-for-linux-@VERSION@-1.opensuse-tumbleweed.x86_64.rpm` | `sudo zypper install --allow-unsigned-rpm ./paint-for-linux-@VERSION@-1.opensuse-tumbleweed.x86_64.rpm` |
| openSUSE Leap 16.0 | `paint-for-linux-@VERSION@-1.opensuse-leap16.0.x86_64.rpm` | `sudo zypper install --allow-unsigned-rpm ./paint-for-linux-@VERSION@-1.opensuse-leap16.0.x86_64.rpm` |
| Any other distribution | `paint-for-linux-@VERSION@-x86_64.AppImage` | `chmod +x paint-for-linux-@VERSION@-x86_64.AppImage`, then run it |
| Any other distribution | `paint-for-linux-@VERSION@-x86_64.tar.gz` | `tar -xzf paint-for-linux-@VERSION@-x86_64.tar.gz`, then run `paint-for-linux-@VERSION@-x86_64/paint-for-linux` |

The distribution packages use the system's Qt and follow the desktop's style. The AppImage and the
tar.gz bring their own Qt and need glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36 and
later); they draw controls in Qt's Fusion style. The packages are not signed: `SHA256SUMS` lists
the checksum of every file.
