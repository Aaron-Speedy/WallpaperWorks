pushd .\third_party\libwebp\
call nmake /f Makefile.vc CFG=release-static RTLIBCFG=static OBJDIR=output
popd
call cl src/wallpaper.c
