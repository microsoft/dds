# Apply patches
patch /opt/mellanox/doca/applications/common/src/dpdk_utils.c ../NetworkEngine/Patches/dpdk_utils.patch

BUILD_DIR="./build/"
if [ -d "$BUILD_DIR" ]; then
	rm -r $BUILD_DIR
fi
meson setup build -Dbuildtype=release
ninja -v -C build
