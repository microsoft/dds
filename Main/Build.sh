# Apply patches
cp ../NetworkEngine/Patches/dpdk_utils.c /opt/mellanox/doca/applications/common/src/dpdk_utils.c

BUILD_DIR="./build/"
if [ -d "$BUILD_DIR" ]; then
	rm -r $BUILD_DIR
fi
meson setup build -Dbuildtype=release
ninja -v -C build
