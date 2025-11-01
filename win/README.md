# to make this work on Fedora 42
sudo dnf install wine.i686 wine-pulseaudio.i686 pulseaudio-libs.i686
WINEARCH=win32 WINEPREFIX=~/.wine winecfg
sudo dnf install wine.x86_64 wine-pulseaudio.x86_64 pulseaudio-libs
WINEPREFIX=~/.wine64 WINEARCH=win64 winecfg
./run-on-wine64 
