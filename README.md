# Network Adapter Class Extension to WDF
The Network Adapter Class Extension to WDF (NetAdapterCx) brings together the productivity of WDF with the networking performance of NDIS.
The goal of NetAdpaterCx is to makes it easy to write a great driver for your NIC.

## Lastest Release - Windows 10 Creators Update (version 1703)

**Source Code to NetAdpaterCx.sys**: Right here

**API Documentation**: [Network Adapter WDF Class Extension (Cx)](https://aka.ms/netadapter/doc)

**Driver Samples**: [Microsoft/NetAdapter-Cx-Driver-Samples](https://github.com/Microsoft/NetAdapter-Cx-Driver-Samples "Driver Samples")

---

## This repository

This repository holds the source code to NetAdpaterCx.sys.  
NetAdapterCx.sys ships with Windows, so you don't need to compile it yourself.
However, you can use this reference source code to debug your own NIC driver, and to learn how NetAdapter works.

While we're proud of our API documentation, we know that even the best docs can't always answer every question you might have.
Sometimes, you just have to refer to the source code.
We've published this code so you can be more productive while developing your own NIC driver.
Our aim is to make the inner workings of NetAdapterCx as transparent as possible.

## Contributing to NetAdapterCx

We welcome bug reports or feature suggestions.
You can file theme here on our GitHub repository's issue tracker, or email us at NetAdapter@microsoft.com.

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
[Microsoft/NetAdapter-Cx-Driver-Samples](https://github.com/Microsoft/NetAdapter-Cx-Driver-Samples "Driver Samples")
