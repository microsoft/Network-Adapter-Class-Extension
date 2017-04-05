# Network Adapter Class Extension to WDF
The Network Adapter Class Extension to WDF (NetAdapterCx) is a set of libraries that make
it simple to write high-quality device drivers.

## Goals for this project

### Learning from the source

Unsure about what a particular NetAdapterCx method is doing? Take a look at the
source. Our aim is to make the inner workings of NetAdapterCx as transparent
for developers as possible.

*Note: As you experiment with NetAdapterCx, you may come across undocumented
 behavior or APIS. We strongly advise against taking dependencies on
 that behavior as it's subject to change in future releases.*

### Debugging with the framework

Using the source in this repo, developers can perform step-through
debugging into the NetAdapterCx source. This makes it much easier to follow
driver activity, understand interactions with the framework, and
diagnose issues.  Debugging can be done live by hooking onto a running
driver or after a crash by analyzing the dump file.  See the
[debugging
page](https://github.com/Microsoft/Windows-Driver-Frameworks/wiki/Debugging-with-WDF-Source
"Debugging with source") in the wiki for instructions.

## Contributing to NetAdapterCx
See
[CONTRIBUTING.md](https://github.com/Microsoft/Windows-Driver-Frameworks/blob/master/CONTRIBUTING.md
"Contributing") for policies on pull-requests to this repo.

## Licensing
NetAdapterCx is licensed under the MIT License.

## Related Repos
Driver samples for NetAdapterCx now also live on GitHub at
[Microsoft/NetAdapter-Cx-Driver-Samples](https://github.com/Microsoft/NetAdapter-Cx-Driver-Samples "Driver Samples")