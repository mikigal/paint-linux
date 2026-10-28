#!/usr/bin/env bash
# Installs a package built by build-package.sh with the distribution's package manager, without
# optional dependencies, and checks that the app starts. Run it as root in a fresh container of
# the distribution the package was built for:
#
#   docker run --rm -v "$PWD/dist:/dist:ro" -v "$PWD/packaging/release:/release:ro" debian:13 \
#       /release/test-package.sh /dist/paint-for-linux_0.1.0-1_debian13_amd64.deb 0.1.0
set -euo pipefail

package=${1:?usage: test-package.sh PACKAGE VERSION}
version=${2:?usage: test-package.sh PACKAGE VERSION}
# shellcheck source=/dev/null
. /etc/os-release

case "$ID" in
arch)
    pacman -Syu --noconfirm
    pacman -U --noconfirm "$package"
    ;;
debian | ubuntu)
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "$package"
    ;;
fedora)
    dnf install -y --setopt=install_weak_deps=False "$package"
    ;;
opensuse-leap | opensuse-tumbleweed)
    zypper --non-interactive install --no-recommends --allow-unsigned-rpm "$package"
    ;;
*)
    echo "test-package: no packaging for $ID" >&2
    exit 1
    ;;
esac

output=$(QT_QPA_PLATFORM=offscreen paint-for-linux --version)
echo "$output"
[[ $output == "paint-for-linux $version" ]]
for file in /usr/share/applications/pl.mikigal.PaintForLinux.desktop \
    /usr/share/metainfo/pl.mikigal.PaintForLinux.metainfo.xml \
    /usr/share/icons/hicolor/scalable/apps/pl.mikigal.PaintForLinux.svg; do
    test -f "$file"
done
