# Remote Read Latency Analysis with tcpdump
## Generate a tcpdump file
- Start a `netsh` capture session by `netsh trace start capture=yes`
- Run remote read unit test
- Stop the capture session by `netsh trace stop`, which generates an `etl` file
- Use [etl2pcapng](https://github.com/microsoft/etl2pcapng/releases) to convert the `etl` file to a `pcapng` file by `.\etl2pcapng.exe [etl path]\NetTrace.etl NetShCapture.pcapng`
- Generate a human readable file with [tcpdump](https://www.microolap.com/products/network/tcpdump/) by `tcpdump -A -r NetShCapture.pcapng > NetShCapture.txt`
- The file is encoded with unicode, so convert it to an ANSI file

## Analyze latencies
- Set the `DUMP_FILE_PATH` variable in `LatencyDistribution.cpp`
- Build the project in Visual Studio and execute