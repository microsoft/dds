BUILD_DIR="./build/"
if [ -d "$BUILD_DIR" ]; then
	rm -r $BUILD_DIR
fi
meson build
ninja -C build
