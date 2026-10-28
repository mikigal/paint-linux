#!/usr/bin/env bash
# Builds the package for the distribution this runs on from the source tree in the current
# directory and puts it in dist/. Run it as root in a fresh container of that distribution; the
# label names the distribution in the file name:
#
#   docker run --rm -v "$PWD:/src" -w /src debian:13 packaging/release/build-package.sh debian13
#
# Arch Linux gets a makepkg package from packaging/arch/PKGBUILD; Debian and Ubuntu a .deb, Fedora
# and openSUSE an .rpm, both from CPack. The build tree stays inside the container.
set -euo pipefail

label=${1:?usage: build-package.sh LABEL}
version=$(packaging/release/version.sh)
# shellcheck source=/dev/null
. /etc/os-release
mkdir -p dist

build() {
    cmake -S . -B /tmp/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_SKIP_INSTALL_RPATH=ON \
        -DPAINT_BUILD_TESTS=OFF \
        -DPAINT_AUTOMATION=OFF \
        -DPAINT_WARNINGS_AS_ERRORS=OFF
    cmake --build /tmp/build
}

# Packages one CPack generator; the remaining arguments are -D overrides for the distribution.
cpack_package() {
    cpack --config /tmp/build/CPackConfig.cmake -B /tmp/cpack -G "$@"
}

# Optional packages differ in name between the RPM distributions.
rpm_package() {
    local recommends=$1 suggests=$2
    build
    cpack_package RPM \
        -D CPACK_RPM_PACKAGE_RECOMMENDS="$recommends" \
        -D CPACK_RPM_PACKAGE_SUGGESTS="$suggests" \
        -D CPACK_RPM_FILE_NAME="paint-for-linux-$version-1.$label.$(uname -m).rpm"
    cp /tmp/cpack/*.rpm dist/
}

case "$ID" in
arch)
    pacman -Syu --noconfirm --needed base-devel cmake ninja qt6-base qt6-svg libzip hicolor-icon-theme
    # makepkg refuses to run as root: build a copy of the tree as an unprivileged user.
    useradd --create-home builder
    mkdir /tmp/src /tmp/pkgdest
    tar --exclude=./build --exclude=./dist --exclude=./.git --exclude=./packaging/arch/src \
        --exclude=./packaging/arch/pkg -cf - . | tar -xf - -C /tmp/src
    chown -R builder /tmp/src /tmp/pkgdest
    su builder -c 'cd /tmp/src/packaging/arch && PKGDEST=/tmp/pkgdest makepkg'
    # Leave out the -debug package makepkg also creates.
    cp /tmp/pkgdest/paint-for-linux-"$version"-*.pkg.tar.zst dist/
    ;;
debian | ubuntu)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends g++ cmake ninja-build pkgconf dpkg-dev file \
        qt6-base-dev qt6-svg-dev libzip-dev libgl-dev
    build
    cpack_package DEB \
        -D CPACK_DEBIAN_PACKAGE_RECOMMENDS="qt6-wayland, qt6-svg-plugins, xdg-desktop-portal" \
        -D CPACK_DEBIAN_PACKAGE_SUGGESTS="kimageformat6-plugins, qt6-image-formats-plugins" \
        -D CPACK_DEBIAN_FILE_NAME="paint-for-linux_$version-1_${label}_$(dpkg --print-architecture).deb"
    cp /tmp/cpack/*.deb dist/
    ;;
fedora)
    dnf install -y gcc-c++ cmake ninja-build pkgconf-pkg-config rpm-build \
        qt6-qtbase-devel qt6-qtsvg-devel libzip-devel
    rpm_package "qt6-qtwayland xdg-desktop-portal" "kf6-kimageformats qt6-qtimageformats"
    ;;
opensuse-leap | opensuse-tumbleweed)
    zypper --non-interactive install gcc-c++ cmake ninja pkgconf rpm-build \
        qt6-base-devel qt6-svg-devel libzip-devel
    rpm_package "qt6-wayland xdg-desktop-portal" "kf6-kimageformats qt6-imageformats"
    ;;
*)
    echo "build-package: no packaging for $ID" >&2
    exit 1
    ;;
esac

ls -l dist
