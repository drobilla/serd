#!/usr/bin/env sh

set -e
set -x

# default configuration
sudo rm -rf build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-x64 meson setup build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-x64 ninja -C build test

# arm32_dbg
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-arm32 meson setup build --cross-file=/usr/share/meson/cross/arm-linux-gnueabihf.ini -Dbuildtype=debug -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-arm32 ninja -C build test

# arm32_rel
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-arm32 meson setup build --cross-file=/usr/share/meson/cross/arm-linux-gnueabihf.ini -Dbuildtype=release -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-arm32 ninja -C build test

# arm64_dbg
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-arm64 meson setup build --cross-file=/usr/share/meson/cross/aarch64-linux-gnu.ini -Dbuildtype=debug -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-arm64 ninja -C build test

# arm64_rel
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-arm64 meson setup build --cross-file=/usr/share/meson/cross/aarch64-linux-gnu.ini -Dbuildtype=release -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-arm64 ninja -C build test

# x64_dbg
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-x64 meson setup build -Dbuildtype=debug -Ddocs=enabled -Dstrict=true -Dwerror=true -Db_coverage=true -Dbindings_py=enabled
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-x64 ninja -C build test
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-x64 ninja -C build coverage-html

# x64_rel
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-x64 meson setup build -Dbuildtype=release -Ddocs=enabled -Dstrict=true -Dwerror=true -Dbindings_py=enabled
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-x64 ninja -C build test

# x64_sanitize
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir -e CC="clang" -e CXX="clang++" -e CFLAGS="-fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=implicit-conversion -fsanitize=local-bounds -fsanitize=nullability" -e CXXFLAGS="-fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=implicit-conversion -fsanitize=local-bounds -fsanitize=nullability" -e LDFLAGS="-fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=implicit-conversion -fsanitize=local-bounds -fsanitize=nullability" lv2plugin/debian-x64-clang meson setup build -Db_lundef=false -Dbuildtype=plain -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-x64-clang ninja -C build test

# mingw32_dbg
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-mingw32 meson setup build --cross-file=/usr/share/meson/cross/i686-w64-mingw32.ini -Dbuildtype=debug -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t -e MESON_TESTTHREADS=1 -e WINEPATH="Z:\\usr\\lib\\gcc\\i686-w64-mingw32\\8.3-win32;Z:\\workdir\\build\\subprojects\\exess" --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-mingw32 ninja -C build test

# mingw32_rel
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-mingw32 meson setup build --cross-file=/usr/share/meson/cross/i686-w64-mingw32.ini -Dbuildtype=release -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t -e MESON_TESTTHREADS=1 -e WINEPATH="Z:\\usr\\lib\\gcc\\i686-w64-mingw32\\8.3-win32;Z:\\workdir\\build\\subprojects\\exess" --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-mingw32 ninja -C build test

# mingw64_dbg
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-mingw64 meson setup build --cross-file=/usr/share/meson/cross/x86_64-w64-mingw32.ini -Dbuildtype=debug -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t -e MESON_TESTTHREADS=1 -e WINEPATH="Z:\\usr\\lib\\gcc\\x86_64-w64-mingw32\\8.3-win32;Z:\\workdir\\build\\subprojects\\exess" --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-mingw64 ninja -C build test

# mingw64_rel
sudo rm -r build
docker run -t --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-mingw64 meson setup build --cross-file=/usr/share/meson/cross/x86_64-w64-mingw32.ini -Dbuildtype=release -Ddocs=disabled -Dstrict=true -Dwerror=true
docker run -t -e MESON_TESTTHREADS=1 -e WINEPATH="Z:\\usr\\lib\\gcc\\x86_64-w64-mingw32\\8.3-win32;Z:\\workdir\\build\\subprojects\\exess" --tmpfs /tmp -v $PWD:/workdir lv2plugin/debian-mingw64 ninja -C build test
