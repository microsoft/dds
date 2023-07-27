# DOCA microbenchmarks on BlueField DPUs

## Compile applications with DOCA library on both DPU and host

Reference: https://docs.nvidia.com/doca/sdk/troubleshooting/index.html#compiling-doca-applications-from-source

- Install CMake: `sudo apt update; sudo apt install cmake`

- Set up PATH on DPU

    - `export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig`

    - `export PATH=${PATH}:/opt/mellanox/doca/tools`

    - `export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig`

- Set up PATH on host

    - `export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig`

    - `export PATH=${PATH}:/opt/mellanox/doca/tools`

    - `export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig`

- Compile: `meson build; ninja -C build`

## Run on DPU

- `doca_regex`

- `doca_compress`

- `doca_comm_channel/cc_server_benchmark`

- `doca_dma/dma_copy_dpu_benchmark`

## Run on host

- `doca_comm_channel/cc_client_benchmark`

- `doca_dma/dma_copy_host_benchmark`

## Set up VFs for DPU-host communication
o
Some applications, e.g., Comm Channel and DMA, require setting up virtual functions in order for DPU and host to communicate. This link is useful: https://docs.nvidia.com/doca/sdk/virtual-functions/index.html.