# WallpaperWorks

Display works of art on your desktop wallpaper.

## Building

Before you're able build WallpaperWorks, you'll have to provide a font
and icon. To do this, first clone the repository and enter it
(see instructions below), then, create a ``resources`` directory in the
project root and copy a ``font.ttf`` and ``favicon.ico`` of your liking
into it. For the best experience, we recommend using an icon encoded at
multiple sizes. You can convert a regular image to an ico file by going
to [icoconverter.com](https://icoconverter.com/), or by using the following
[ImageMagick](https://imagemagick.org/) command.

```sh
convert in.jpg -define icon:auto-resize=32,48,64,128,256 -compress zip favicon.ico
```

### Windows

To build on Windows, first install Git, then download and launch
[w64devkit](github.com/skeeto/w64devkit). Before you build, you must
provide resources (see instructions above). Once you have everything
in order, run

```sh
git clone https://github.com/Aaron-Speedy/WallpaperWorks
cd WallpaperWorks
./configure.sh # This should take about 10 minutes
./build.sh
```

If that works, congratulations! To run it, move into the build directory
and run ``./WallpaperWorks.exe``. We also provide an installer. To install
the program, first install [NSIS](https://nsis.sourceforge.io/Main_Page),
then run

```sh
./build_installer.sh
./build/WallpaperWorksInstaller.exe
```
