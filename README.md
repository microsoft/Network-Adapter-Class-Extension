# Network Adapter Class Extension to WDF
The Network Adapter Class Extension to WDF (NetAdapterCx) brings together the productivity of WDF with the networking performance of NDIS.
The goal of NetAdpaterCx is to make it easy to write a great driver for your NIC.

## Lastest Release - Windows 10 April 2020 Update (version 2004)

**Source Code to NetAdpaterCx.sys**: Right here

**API Documentation**: [Network Adapter WDF Class Extension (Cx)](https://aka.ms/netadapter/doc)

**Visual Studio 2017**: [Visual Studio 2017 Download](https://www.visualstudio.com/downloads/)

**Windows 10 WDK**: [Windows 10 WDK Download](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)

**Windows 10 SDK**: [Windows 10 SDK Download](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk)

**Driver Samples**: [Microsoft/NetAdapter-Cx-Driver-Samples](https://github.com/Microsoft/NetAdapter-Cx-Driver-Samples "Driver Samples")

---

## What's this code good for?

This repository holds the source code to NetAdpaterCx.sys.  
NetAdapterCx.sys ships with Windows, so you don't need to compile it yourself.
However, you can use this reference source code to debug your own NIC driver, and to learn how NetAdapter works.

While we're proud of our API documentation, we know that even the best docs can't always answer every question you might have.
Sometimes, you just have to refer to the source code.
We've published this code so you can be more productive while developing your own NIC driver.
Our aim is to make the inner workings of NetAdapterCx as transparent as possible.

## An overview of the code layout

The code has several major pieces:

    rtl\inc     Runtime library utility headers
    cx\sys      The core NetAdapterCx.sys implementation
    cx\xlat     The datapath

### The runtime library

NetAdapterCx.sys uses a few simple utility headers to wrap low-level kernel calls.
For example, KSpinLock.h provides a convenient wrapper around the kernel's native spinlock.
KRegKey.h provides convenient C++ wrappers to access the registry.

### The core NetAdapterCx.sys implementation

This code implements all the public API surface of NetAdapterCx.
There is a naming convention to the files, organized around the objects in the public API.
For example, consider the NETCONFIGURATION object.  Then:
* NxConfigurationApi.cpp holds entrypoints for each public API method exposed by the NETCONFIGURATIONOBJECT.  These entrypoints convert handles, validate parameters, and generally hold mechanical set-up work before jumping to the actual implementation.  Start here, if you need to know why NetAdapter fires a verifier bugcheck when you make an API call.
* NxConfiguration.cpp holds the implementation a C++ class that encapsulates the actual logic.  Start here, if you need to know why an API call behaves the way it does.
* NxConfiguration.hpp is the associated declaration of the C++ class.

### The datapath

The datapath is currently under heavy development, so its implementation is likely to change substantially in future releases.

The core of the transmit path is in NxTxXlat.cpp, centered around the `NxTxXlat::TransmitThread` function.
This function drives the transmit datapath.
Each time NDIS gives NBLs to the network driver, the NBLs are queued for the TransmitThread.
The TransmitThread converts the NBLs to NET_PACKETs and issues them to your NIC driver.

The core of the receive path is in NxRxXlat.cpp, centered around the `NxRxXlat::ReceiveThread` function.
This routine polls your NIC for new packets.
When new packets are available, the ReceiveThread converts them to NBLs and indicates them to NDIS.

## Contributing to NetAdapterCx

We welcome bug reports and feature suggestions.
You can file them here on our GitHub repository's issue tracker, or email us at NetAdapter@microsoft.com.

NetAdapterCx.sys is under heavy development, so its implementation is changing quite a bit.
Currently, we do that development internally at Microsoft, so you won't see the in-progress changes here on GitHub.
The code here is a snapshot of the last shipping version of NetAdapterCx.
That means we cannot take pull requests through GitHub.
If you would like to collaborate with us on the code, please contact us first.

This project has adopted the [Microsoft Open Source Code of
Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct
FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com)
with any additional questions or comments.

## Licensing
The code hosted here is licensed under the MIT License.

## Related Repos
Driver samples for NetAdapterCx now also live on GitHub at

- [Microsoft/NetAdapter-Cx-Driver-Samples](https://github.com/Microsoft/NetAdapter-Cx-Driver-Samples "Driver Samples")
- [Microsoft/NCM-Driver-for-Windows](https://github.com/Microsoft/NCM-Driver-for-Windows)

