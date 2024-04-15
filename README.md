# Acquire common libraries

[![Build](https://github.com/acquire-project/acquire-common/actions/workflows/build.yml/badge.svg)](https://github.com/acquire-project/acquire-common/actions/workflows/build.yml)
[![Tests](https://github.com/acquire-project/acquire-common/actions/workflows/test.yml/badge.svg)](https://github.com/acquire-project/acquire-common/actions/workflows/test.yml)

## Components

### Acquire Core Libraries

This builds the core static libraries that get used in other parts of Acquire.
For the most part, these are interfaces private to Acquire, although there are
some exceptions.

- **acquire-core-logger** Logging facility.
- **acquire-core-platform** Enables compatibility across operating systems.
- **acquire-device-properties** Defines the parameters used for configuring devices.
- **acquire-device-kit** Defines the API needed for implementing a new device driver.
- **acquire-device-hal** Defines the API the runtime uses for interacting with drivers and devices.

#### Acquire core logger

Acquire reports debug and trace events by printing them through a "logger".
This library defines what that is and provides a function for setting a global
callback at runtime that handles incoming messages.

This involves very little code. It barely merits being its own library. It's
here to define what the expected callback interface is - something almost every
component needs to know about.

#### Acquire core platform

The Acquire core platform library defines a platform-independent interface for
interacting with the linux, osx, or windows operating systems.

This is used internally with certain Acquire components.

#### Acquire device properties

This library defines many of the types that the Acquire device interfaces rely on to configure and query devices.

#### Acquire Device Kit

This is a header only library. Some types are defined outside this library -
it needs to be combined with `acquire-device-properties` to be fully defined.

Acquire relies on device adapters to define video sources, filters and sinks.
The Device Kit defines the interfaces that need to be implemented to create a
device adapter.

Each device adapter is compiled to a shared library that exposes a single
function, `acquire_driver_init_v0()`. That function returns a "Driver"
instance. Here, a Driver's responsibility is to list a number of devices that
can be opened and to manage those devices as resources. It also manages those
resources.

The "Devices" themselves may implement specific interfaces depending on their
Kind. See `storage.h` and `camera.h` for details.

#### Acquire Hardware Abstraction Library (HAL)

The Acquire HAL defines internal (private) functionality that the runtime uses
to interface with different kinds of devices.

### Acquire Video Runtime

This is a multi-camera video streaming library _focusing_ on cameras and file-formats for microscopy.

This is the video runtime for the Acquire project.

## Acquire Common Driver

This is an Acquire Driver that exposes commonly used devices.

#### Camera devices

- **simulated: uniform random** - Produces uniform random noise for each pixel.
- **simulated: radial sin** - Produces an animated radial sin-wave pattern.
- **simulated: empty** - Produces no data, leaving image buffers blank. Simulates going as fast as possible.

#### Storage devices

- **raw** - Streams to a raw binary file.
- **tiff** - Streams to a [bigtiff] file. Metadata is stored in the `ImageDescription` tag for each frame as a `json`
  string.
- **tiff-json** - Stores the video stream in a *bigtiff* (as above) and stores metadata in a `json` file. Both are
  located in a folder identified by the `uri` property.
- **Trash** - Writes nothing. Discards incoming data.

[bigtiff]: http://bigtiff.org/

## Platform support

These libraries support Windows, OSX and Linux.

Windows support is the highest priority and has the widest device compatibility. OSX and Linux are actively tested.

## Compiler support

The primary compiler targeted is MSVC's toolchain.

## Build environment

1. Install CMake 3.23 or newer
2. Install a fairly recent version of Microsoft Visual Studio.

From the repository root:

```
mkdir build
cd build
cmake-gui ..
```

## Build

From the repository root:

```
cmake --build build
```

## Using [pre-commit](https://pre-commit.com/)

Pre-commit is a utility that runs certain checks before you can create a commit
in git. This helps ensure each commit addresses any quality/formatting issues.

To use, first install it. It's a python package so you can use `pip`.

```
pip install pre-commit
```

Then, navigate to this repo and initialize:

```
cd acquire-common
pre-commit install
```

This will configure the git hooks so they get run the next time you try to commit.

> **Tips**
>
> - The formatting checks modify the files to fix them.
> - The commands that get run are in `.pre-commit-config.yaml`
    > You'll have to install those.
> - `cppcheck` is disabled by default, but can be enabled by uncommenting the
    > corresponding lines in `.pre-commit-config.yaml`. See that file for more
    > details.