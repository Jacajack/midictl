# Maintainer: None
pkgname=midictl
pkgver=v0.9
pkgrel=1
epoch=
pkgdesc="Terminal-based MIDI control panel meant to be fast and simple in use"
arch=('x86' 'x86_64')
url="https://github.com/jacajack/midictl"
license=('MIT')
groups=()
depends=('alsa-lib' 'ncurses')
makedepends=('gcc' 'make')
checkdepends=()
provides=()
conflicts=()
replaces=()
source=("midictl-src::git+https://www.github.com/jacajack/midictl#tag=v0.9")
cksums=(SKIP)

prepare() {
	:
}

build() {
	cd "midictl-src"
	make
}

package() {
	cd "midictl-src"
	mkdir -p "$pkgdir/usr/bin"
	cp midictl "$pkgdir/usr/bin/"
}
