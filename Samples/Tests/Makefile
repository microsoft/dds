CC=gcc

SPDK_DIR=~/spdk
PKG_CONFIG_PATH=$(SPDK_DIR)/build/lib/pkgconfig
$(info PKG_CONFIG_PATH: $(PKG_CONFIG_PATH))

# SPDK_LIB := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs spdk_bdev)
# DPDK_LIB := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs spdk_env_dpdk)
# $(info SPDK_LIB: $(SPDK_LIB))
# $(info DPDK_LIB: $(DPDK_LIB))
# LDLIBS=-pthread -Wl,--no-as-needed -lspdk -lspdk_env_dpdk $(SPDK_LIB) $(DPDK_LIB)

# SPDK_LIB := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs spdk_bdev)
# DPDK_LIB := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs spdk_env_dpdk)
# SYS_LIB := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs --static spdk_syslibs)

# LDLIBS=-pthread -Wl,--whole-archive $(SPDK_LIB) $(DPDK_LIB) $(SYS_LIB)

LDLIBS=-pthread -lspdk -lspdk_env_dpdk -ldpdk

Include=-I/home/jason/spdk/include #-I../../DPDPU/Common/Include -I../../DPDPU/StorageEngine/DDSBackEndDPUService/Include
CFLAGS=-std=gnu11 -O3

PROG=TestProg
BUILD_DIR=./build
SRC_ROOT=../../DPDPU

SRCS = test.c \
	../../DPDPU/StorageEngine/DDSBackEndDPUService/Source/DPUBackEndStorage.c \
	../../DPDPU/StorageEngine/DDSBackEndDPUService/Source/DPUBackEndFile.c \
	../../DPDPU/StorageEngine/DDSBackEndDPUService/Source/DPUBackEndDir.c \
	../../DPDPU/StorageEngine/DDSBackEndDPUService/Source/bdev.c

OBJ = $(SRCS:%.c=$(BUILD_DIR)/%.o)
$(info OBJ: $(OBJ))
DEP = $(OBJ:%.o=%.d)


$(PROG) : $(BUILD_DIR)/$(PROG)

# Actual target of the binary - depends on all .o files.
$(BUILD_DIR)/$(PROG) : $(OBJ)
	mkdir -p $(@D)
	$(CC) $(Include) $(CFLAGS) $^ -o $@ $(LDLIBS)

-include $(DEP)

# Build target for every single object file.
# The potential dependency on header files is covered
# by calling `-include $(DEP)`.
# The -MMD flags additionaly creates a .d file with
# the same name as the .o file.
$(BUILD_DIR)/%.o : %.c
	$(info with -MMD: $<)
	mkdir -p $(@D)
	$(CC) $(Include) $(CFLAGS) -MMD -c $< -o $@ $(LDLIBS)

.PHONY: clean
clean:
	# This should remove all generated files.
	-rm $(BUILD_DIR)/$(BIN) $(OBJ) $(DEP)

all: $(PROG)