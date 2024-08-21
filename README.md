DDS is a disaggregated storage architecture for cloud data systems augmented by data processing units (DPUs).
It minimizes the resource consumption and latency of storage servers while improving throughput by offloading remote storage requests to the DPU.

Details of the system can be found in [our VLDB 2024 paper](https://fardatalab.org/vldb24-zhang.pdf).

# Dependencies
## DPU (BlueField-2)
- DOCA 1.5.1
- DPDK 20.11.7.1.2 (default version in DOCA 1.5.1)
- SPDK v22.05 (included as a submodule)


## Host

- Windows Server 2022

- [Visual Studio 2019](https://learn.microsoft.com/en-us/windows-hardware/drivers/other-wdk-downloads#step-1-install-visual-studio)  

	Make sure to select the following workloads during installation:

	- .NET desktop development (required for CBT/Nuget packages)
	- Desktop development with C++  
	</br>

	Also, make sure to install `MSVC build tools` and `MSVC Spectre-mitigated libraries` (`MSVC v142 - VS 2019 C++ x64/x86 build tools` and `MSVC v142 - VS 2019 C++ x64/x86 Spectre-mitigated libs` were tested).

- Windows SDK 10.0 (which comes with Visual Studio 2019)

- [WDK for Windows Server 2022](https://learn.microsoft.com/en-us/windows-hardware/drivers/other-wdk-downloads#step-2-install-the-wdk)

- [.NET SDK v5.0](https://dotnet.microsoft.com/en-us/download/dotnet/5.0#sdk-5.0.408-5.0.17)

# Sample Application
This repo provides a disaggregated storage application `AppDisagg` where the client issues file read/write requests that will be sent to and serviced by the server.

With DDS, file read requests will be offloaded to the DPU equipped on the server machine.

# Build

## DDS
On the BlueField-2 DPU, DDS can be built with `Main/Build.sh`.

## Server host
On the server host in Windows, the server application under `AppDisagg/ServerDDS` can be built with Visual Studio 2019 using the `DisaggStore` solution.

## Client
On the client machine in Windows, the client application under `AppDisagg/Client` can be built with Visual Studio 2019 using the `DisaggStore` solution.


# Configure

- Host: APR entry for the DPU must be statically configured
	- Windows: (1) `Get-NetAdapter` to find the index of the BF-2 CX-6 NIC, and (2) `New-NetNeighbor -InterfaceIndex [BF-2 CX-6 NIC index] -IPAddress '[DPU IP]' -LinkLayerAddress '000000000100' -State Permanent`
	- Linux: run `Scripts/HostAddArpLinux.sh [DPU IP] 00:00:00:00:01:00`

- DPU: the scalable function id in `Main/NetworkConfig.json` must be consistent with the created scalable function for DDS (check with `devlink dev show`). Other parameters in `Main/StorageConfig.json` and `Main/NetworkConfig.json` must also be properly configured.

# Run
## DDS
`Main/Run.sh` runs DDS based on the configurations specified in `Main/NetworkConfig.json` and `Main/StorageConfig.json`. 

## Server host
`ServerDDS.exe` under the build folder runs the application server with the following arguments: `[access size (bytes)] [batch size] [queue depth] [number of requests] [file size (GB)] [number of files] [number of connections] [number of completion threads]`.

## Client
`Client.exe` under the build folder runs the application client with two modes:
- Latency measurement: `[access size (bytes)] [batch size] [queue depth] [number of requests] [local port] [file size (GB)]`
- Throughput measurement: `[access size (bytes)] [batch size] [queue depth] [number of requests] [local port] [number of connections] [file size (GB)] [number of files]`