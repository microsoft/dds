cd ../../ThirdParty/spdk
patch lib/env_dpdk/init.c ../../StorageEngine/DDSBackEndDPUService/Patches/spdk_dpdk_init.patch

CFLAGS="$(pkg-config --cflags libdpdk)" make -j
make install

cd -
rm -r build; meson setup build -Dbuildtype=release; ninja -v -C build
