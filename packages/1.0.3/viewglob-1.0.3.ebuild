# Copyright 1999-2004 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header$

DESCRIPTION="A shell add-on for windowing environments"
HOMEPAGE="http://viewglob.sourceforge.net/"
SRC_URI="mirror://sourceforge/viewglob/${P}.tar.gz"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~x86"
IUSE=""

RESTRICT="nomirror"

DEPEND="app-shells/bash
	>=x11-libs/gtk+-2.4.0"

src_compile() {
	econf || die "econf failed"
	emake || die "emake failed"
}

src_install () {
	make DESTDIR=${D} install || die "install failed"
	dodoc AUTHORS COPYING COPYING2 ChangeLog HACKING INSTALL NEWS README TODO
}

