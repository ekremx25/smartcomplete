# Maintainer: ekremx25 <https://github.com/ekremx25>
pkgname=linuxcomplete
pkgver=0.1.0
pkgrel=1
pkgdesc="System-wide intelligent text prediction engine for Linux (Wayland)"
arch=('x86_64')
url="https://github.com/ekremx25/linuxcomplete"
license=('MIT')
depends=('fcitx5' 'nlohmann-json')
makedepends=('cmake' 'gcc')
source=("git+https://github.com/ekremx25/linuxcomplete.git")
sha256sums=('SKIP')

build() {
    cd "$srcdir/$pkgname"
    mkdir -p build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr
    make -j"$(nproc)"
}

package() {
    cd "$srcdir/$pkgname/build"
    make DESTDIR="$pkgdir" install
    install -Dm644 "$srcdir/$pkgname/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
