#!/usr/bin/env bash
# Builds the AppImage and a portable tar.gz of the same files from the source tree in the current
# directory and puts them in dist/. Both bundle Qt from qt.io and the libraries it needs. Run it as
# root in an Ubuntu 22.04 container, so they work on any distribution with glibc 2.35 or newer:
#
#   docker run --rm -v "$PWD:/src" -w /src ubuntu:22.04 packaging/release/build-appimage.sh
set -euo pipefail

qt_version=6.12.0
version=$(packaging/release/version.sh)
name=paint-for-linux-$version-x86_64
mkdir -p dist

export DEBIAN_FRONTEND=noninteractive
apt-get update
# The compiler and headers, then the libraries Qt's xcb, Wayland and print support plugins load,
# which linuxdeploy bundles unless every desktop has them.
apt-get install -y --no-install-recommends g++-12 make pkg-config python3-venv curl ca-certificates \
    file libzip-dev libgl-dev libegl-dev libvulkan-dev libxkbcommon-dev libfontconfig1-dev \
    libglib2.0-0 libdbus-1-3 libcups2 libx11-xcb1 libxkbcommon-x11-0 libxcb-cursor0 \
    libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 \
    libxcb-xinput0 libxcb-xkb1 libwayland-client0 libwayland-cursor0 libwayland-egl1

# Ubuntu 22.04's CMake is too old; Qt comes from qt.io through aqtinstall.
python3 -m venv /tmp/venv
/tmp/venv/bin/pip install --quiet aqtinstall cmake ninja
export PATH=/tmp/venv/bin:$PATH
aqt install-qt linux desktop "$qt_version" linux_gcc_64 --modules qtimageformats --outputdir /opt/qt
qt=/opt/qt/$qt_version/gcc_64

cmake -S . -B /tmp/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_PREFIX_PATH="$qt" \
    -DCMAKE_CXX_COMPILER=g++-12 \
    -DPAINT_BUILD_TESTS=OFF \
    -DPAINT_AUTOMATION=OFF \
    -DPAINT_WARNINGS_AS_ERRORS=OFF
cmake --build /tmp/build
DESTDIR=/tmp/AppDir cmake --install /tmp/build

mkdir /tmp/tools /tmp/out
for tool in linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage; do
    curl -fsSL -o "/tmp/tools/${tool##*/}" "https://github.com/linuxdeploy/$tool"
done
chmod +x /tmp/tools/*.AppImage
export PATH=/tmp/tools:$PATH

# Containers have no FUSE to mount AppImages with.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=$qt/bin/qmake
export LD_LIBRARY_PATH=$qt/lib
# xcb is always deployed; Wayland brings its shell and decoration plugins along. Offscreen lets
# the app run headless (--version in CI).
export EXTRA_PLATFORM_PLUGINS="libqwayland.so;libqoffscreen.so"
export LDAI_OUTPUT=$name.AppImage
export LDAI_UPDATE_INFORMATION="gh-releases-zsync|mikigal|paint-linux|latest|paint-for-linux-*-x86_64.AppImage.zsync"
cd /tmp/out
linuxdeploy-x86_64.AppImage --appdir /tmp/AppDir --plugin qt --output appimage \
    --desktop-file /tmp/AppDir/usr/share/applications/pl.mikigal.PaintForLinux.desktop \
    --icon-file /tmp/AppDir/usr/share/icons/hicolor/scalable/apps/pl.mikigal.PaintForLinux.svg
cd - >/dev/null

# Without these the app cannot open windows on Wayland.
for plugin in platforms/libqwayland.so wayland-shell-integration/libxdg-shell.so; do
    test -f "/tmp/AppDir/usr/plugins/$plugin"
done

# The tar.gz is the same tree: unpack anywhere and run paint-for-linux in it.
cp -a /tmp/AppDir "/tmp/$name"
ln -s AppRun "/tmp/$name/paint-for-linux"
tar -C /tmp -czf "dist/$name.tar.gz" "$name"
cp "/tmp/out/$name.AppImage" "/tmp/out/$name.AppImage.zsync" dist/
chmod 755 "dist/$name.AppImage"

ls -l dist
