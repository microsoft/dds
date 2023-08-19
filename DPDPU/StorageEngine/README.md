# Host dependencies

- Windows Server 2022

- [Visual Studio 2019](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk#download-icon-step-1-install-visual-studio-2019)  

	Make sure to select the following workloads during installation:

	- .NET desktop development (required for CBT/Nuget packages)
	- Desktop development with C++  
	</br>

	Also, make sure to install `MSVC build tools` and `MSVC Spectre-mitigated libraries` (`MSVC v142 - VS 2019 C++ x64/x86 build tools` and `MSVC v142 - VS 2019 C++ x64/x86 Spectre-mitigated libs` were tested).

- Windows SDK 10.0 (which comes with Visual Studio 2019)

- [WDK for Windows Server 2022](https://learn.microsoft.com/en-us/windows-hardware/drivers/other-wdk-downloads)

- [.NET SDK v5.0.408](https://dotnet.microsoft.com/en-us/download/dotnet/5.0)