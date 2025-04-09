Composer
========

Composer is a song editor for creating (and converting) notes for music games in various formats. It attempts to make the process easy by automating as much as possible while providing a simple and attractive interface to do the remaining manual work.

Homepage: http://performous.org/composer

Features
--------

* Song pitch analysis based on the esteemed algorithms from Performous.
* Zoomable interface to quickly get an overview or doing very precise timing.
* Possibility to synthesize the notes to get a feel of their "sound".
* Import/export in various formats including:
	- SingStar XML
	- UltraStar TXT
	- Frets on Fire MIDI
	- LRC

Latest builds
==========
- [Windows (MSVC)](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-msvc.exe.zip)
- [Windows (MinGW-w64)](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-mingw-w64.exe.zip)
- [Linux - Ubuntu 20.04](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-ubuntu_20.04.deb.zip)
- [Linux - Ubuntu 22.04](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-ubuntu_22.04.deb.zip)
- [Linux - Ubuntu 24.04](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-ubuntu_24.04.deb.zip)
- [Linux - Debian 12](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-debian_12.deb.zip)
- [Linux - Fedora 36](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-fedora_36.rpm.zip)
- [Linux - Fedora 37](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-fedora_37.rpm.zip)
- [Linux - Fedora 38](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-fedora_38.rpm.zip)
- [Linux - Fedora 39](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-fedora_39.rpm.zip)
- [Linux - Fedora 40](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-fedora_40.rpm.zip)
- [Linux - Fedora 41](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest-fedora_41.rpm.zip)
- [Linux - AppImage](https://nightly.link/performous/composer/workflows/build_and_release/master/Composer-latest.AppImage.zip)

Build & Install
--------

For building the master branch you will need:
Qt5:
Core, Gui, Widgets, Network, XML, Multimedia, Multimedia-widgets, Multimedia-plugins and Platform files

FFmpeg/Libav:
LibAVCodec, LibAVUtil, LibAVFormat, LibSWResample

Other:
Zlib, Xvid, ssleay, libxml, libx264, libvorbis, libtheora, libspeex, libpng, libstdc++, libpcre,
libopus, libopencore, libogg, libnettle, libmp3lame, liblzma, libintl, libiconv, libhogweed, libharfbuzz, libgnutls, libgmp, glib, libgcc, libfreetype, libeay32, libbz2, libbluray, icuuc, icuin, icudt.

Build for linux:
To build for linux simply install the required libraries through your distribution's package manager along with CMake. Then create a build folder and use cmake (or cmake-gui) to generate your makefiles. Then make && make install (last command might require root privileges).

Build for Windows:
To build for Windows simply install the required libraries through vcpkg. Then startup Visual Studio and let cmake generate your makefiles. Then build the project and make it run.
