# Build Instructions
This document contains the instructions for building this repository on Linux and Windows.

This repository does not contain a Vulkan-capable driver.
Before proceeding, it is strongly recommended that you obtain a Vulkan driver from your graphics hardware vendor
and install it.

## Contributing

If you intend to contribute, the preferred work flow is for you to develop your contribution
in a fork of this repo in your GitHub account and then submit a pull request.
Please see the [CONTRIBUTING](CONTRIBUTING.md) file in this repository for more details.

## Git the Bits

To create your local git repository:
```
git clone https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers
```

## Linux Build

The build process uses CMake to generate makefiles for this project.
The build generates the loader, layers, and tests.

This repo has been built and tested on the two most recent Ubuntu LTS versions.
It should be straightforward to use it on other Linux distros.

These packages are needed to build this repository:
```
sudo apt-get install git cmake build-essential bison libx11-xcb-dev libxkbcommon-dev libmirclient-dev libwayland-dev libxrandr-dev
```

Example debug build (Note that the update\_external\_sources script used below builds external tools into predefined locations. See **Loader and Validation Layer Dependencies** for more information and other options):
```
cd Vulkan-LoaderAndValidationLayers  # cd to the root of the cloned git repository
./update_external_sources.sh
cmake -H. -Bdbuild -DCMAKE_BUILD_TYPE=Debug
cd dbuild
make
```

If you have installed a Vulkan driver obtained from your graphics hardware vendor, the install process should
have configured the driver so that the Vulkan loader can find and load it.

If you want to use the loader and layers that you have just built:
```
export LD_LIBRARY_PATH=<path to your repository root>/dbuild/loader
export VK_LAYER_PATH=<path to your repository root>/dbuild/layers
```
You can run the `vulkaninfo` application to see which driver, loader and layers are being used.

The `LoaderAndLayerInterface` document in the `loader` folder in this repository is a specification that
describes both how ICDs and layers should be properly
packaged, and how developers can point to ICDs and layers within their builds.

### WSI Support Build Options
By default, the Vulkan Loader and Validation Layers are built with support for all 4 Vulkan-defined WSI display systems, Xcb, Xlib, Wayland, and Mir.  It is recommended to build these modules with support for these
display systems to maximize their usability across Linux platforms.
If it is necessary to build these modules without support for one of the display systems, the appropriate CMake option of the form BUILD_WSI_xxx_SUPPORT can be set to OFF.   See the top-level CMakeLists.txt file for more info.

### Linux Install to System Directories

Installing the files resulting from your build to the systems directories is optional since
environment variables can usually be used instead to locate the binaries.
There are also risks with interfering with binaries installed by packages.
If you are certain that you would like to install your binaries to system directories,
you can proceed with these instructions.

Assuming that you've built the code as described above and the current directory is still `dbuild`,
you can execute:

```
sudo make install
```

This command installs files to:

* `/usr/local/include/vulkan`:  Vulkan include files
* `/usr/local/lib`:  Vulkan loader and layers shared objects
* `/usr/local/bin`:  vulkaninfo application
* `/usr/local/etc/vulkan/explicit_layer.d`:  Layer JSON files

You may need to run `ldconfig` in order to refresh the system loader search cache on some Linux systems.

The list of installed files appears in the build directory in a file named `install_manifest.txt`.
You can easily remove the installed files with:

```
cat install_manifest.txt | sudo xargs rm
```

You can further customize the installation location by setting additional CMake variables
to override their defaults.
For example, if you would like to install to `/tmp/build` instead of `/usr/local`, specify:

```
-DCMAKE_INSTALL_PREFIX=/tmp/build
-DDEST_DIR=/tmp/build
```

on your CMake command line and run `make install` as before.
The install step places the files in `/tmp/build`.

Using the `CMAKE_INSTALL_PREFIX` to customize the install location also modifies the
loader search paths to include searching for layers in the specified install location.
In this example, setting `CMAKE_INSTALL_PREFIX` to `/tmp/build` causes the loader to
search `/tmp/build/etc/vulkan/explicit_layer.d` and `/tmp/build/share/vulkan/explicit_layer.d`
for the layer JSON files.
The loader also searches the "standard" system locations of `/etc/vulkan/explicit_layer.d`
and `/usr/share/vulkan/explicit_layer.d` after searching the two locations under `/tmp/build`.

You can further customize the installation directories by using the CMake variables
`CMAKE_INSTALL_SYSCONFDIR` to rename the `etc` directory and `CMAKE_INSTALL_DATADIR`
to rename the `share` directory.

See the CMake documentation for more details on using these variables
to further customize your installation.

Also see the `LoaderAndLayerInterface` document in the `loader` folder in this repository for more
information about loader operation.

Note that some executables in this repository (e.g., `cube`) use the "rpath" linker directive
to load the Vulkan loader from the build directory, `dbuild` in this example.
This means that even after installing the loader to the system directories, these executables
still use the loader from the build directory.

### Linux 32-bit support

Usage of this repository's contents in 32-bit Linux environments is not officially supported.
However, since this repository is supported on 32-bit Windows, these modules should generally
work on 32-bit Linux.

Here are some notes for building 32-bit targets on a 64-bit Ubuntu "reference" platform:

If not already installed, install the following 32-bit development libraries:

`gcc-multilib g++-multilib libx11-dev:i386`

This list may vary depending on your distro and which windowing systems you are building for.

Set up your environment for building 32-bit targets:

```
export CFLAGS=-m32
export CXXFLAGS=-m32
export PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu
```

Again, your PKG_CONFIG configuration may be different, depending on your distro.

If the libraries in the `external` directory have already been built
for 64-bit targets,
delete or "clean" this directory and rebuild it with
the above settings using the `update_external_sources` shell script.
This is required because the libraries in `external` must be built for
32-bit in order to be usable by the rest of the components in the repository.

Finally, rebuild the repository using `cmake` and `make`, as explained above.

## Validation Test

The test executables can be found in the dbuild/tests directory.
Some of the tests that are available:
- vk\_layer\_validation\_tests: Test Vulkan layers.

There are also a few shell and Python scripts that run test collections (eg,
`run_all_tests.sh`).

## Linux Demos

Some demos that can be found in the dbuild/demos directory are:
- vulkaninfo: report GPU properties
- cube: a textured spinning cube
- smoke/smoke: A "smoke" test using a more complex Vulkan demo

You can select which WSI subsystem is used to build the demos using a cmake option called DEMOS_WSI_SELECTION.
Supported options are XCB (default), XLIB, WAYLAND, and MIR.  Note that you must build using the corresponding BUILD_WSI_*_SUPPORT enabled at the base repo level (all SUPPORT options are ON by default).
For instance, creating a build that will use Xlib to build the demos, your cmake command line might look like:

cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug -DDEMOS_WSI_SELECTION=XLIB

## Windows System Requirements

Windows 7+ with additional required software packages:

- Microsoft Visual Studio 2013 Professional.  Note: it is possible that lesser/older versions may work, but that has not been tested.
- [CMake](http://www.cmake.org/download/).  Notes:
  - Tell the installer to "Add CMake to the system PATH" environment variable.
- [Python 3](https://www.python.org/downloads).  Notes:
  - Select to install the optional sub-package to add Python to the system PATH environment variable.
  - Ensure the pip module is installed (it should be by default)
  - Need python3.3 or later to get the Windows py.exe launcher that is used to get python3 rather than python2 if both are installed on Windows
  - 32 bit python works
- [Git](http://git-scm.com/download/win).
  - Note: If you use Cygwin, you can normally use Cygwin's "git.exe".  However, in order to use the "update\_external\_sources.bat" script, you must have this version.
  - Tell the installer to allow it to be used for "Developer Prompt" as well as "Git Bash".
  - Tell the installer to treat line endings "as is" (i.e. both DOS and Unix-style line endings).
  - Install each a 32-bit and a 64-bit version, as the 64-bit installer does not install the 32-bit libraries and tools.
- glslang is required for demos and tests.
  - [You can download and configure it (in a peer directory) here](https://github.com/KhronosGroup/glslang/blob/master/README.md)
  - A windows batch file has been included that will pull and build the correct version.  Run it from Developer Command Prompt for VS2013 like so:
    - update\_external\_sources.bat --build-glslang (Note: see **Loader and Validation Layer Dependencies** below for other options)

## Windows Build - MSVC

Before building on Windows, you may want to modify the customize section in loader/loader.rc to so as to
set the version numbers and build description for your build. Doing so will set the information displayed
for the Properties->Details tab of the loader vulkan-1.dll file that is built.

Build all Windows targets after installing required software and cloning the Loader and Validation Layer repo as described above by completing the following steps in a "Developer Command Prompt for VS2013" window (Note that the update\_external\_sources script used below builds external tools into predefined locations. See **Loader and Validation Layer Dependencies** for more information and other options):
```
cd Vulkan-LoaderAndValidationLayers  # cd to the root of the cloned git repository
update_external_sources.bat --all
build_windows_targets.bat
```

At this point, you can use Windows Explorer to launch Visual Studio by double-clicking on the "VULKAN.sln" file in the \build folder.  Once Visual Studio comes up, you can select "Debug" or "Release" from a drop-down list.  You can start a build with either the menu (Build->Build Solution), or a keyboard shortcut (Ctrl+Shift+B).  As part of the build process, Python scripts will create additional Visual Studio files and projects, along with additional source files.  All of these auto-generated files are under the "build" folder.

Vulkan programs must be able to find and use the vulkan-1.dll library. Make sure it is either installed in the C:\Windows\System32 folder, or the PATH environment variable includes the folder that it is located in.

To run Vulkan programs you must tell the icd loader where to find the libraries.
This is described in a `LoaderAndLayerInterface` document in the `loader` folder in this repository.
This specification describes both how ICDs and layers should be properly
packaged, and how developers can point to ICDs and layers within their builds.

## Android Build
Install the required tools for Linux and Windows covered above, then add the
following.
### Android Studio
- Install [Android Studio 2.1](http://tools.android.com/download/studio/canary), latest Preview (tested with 4):
- From the "Welcome to Android Studio" splash screen, add the following components using Configure > SDK Manager:
  - SDK Platforms > Android N Preview
  - SDK Tools > Android NDK

#### Add NDK to path

On Linux:
```
export PATH=$HOME/Android/sdk/ndk-bundle:$PATH
```
On Windows:
```
set PATH=%LOCALAPPDATA%\Android\sdk\ndk-bundle;%PATH%
```
On OSX:
```
export PATH=$HOME/Library/Android/sdk/ndk-bundle:$PATH
```
### Additional OSX System Requirements
Tested on OSX version 10.11.4

 Setup Homebrew and components
- Follow instructions on [brew.sh](http://brew.sh) to get homebrew installed.
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
- Ensure Homebrew is at the beginning of your PATH:
```
export PATH=/usr/local/bin:$PATH
```
- Add packages with the following (may need refinement)
```
brew install cmake python python3 git
```
### Build steps for Android
Use the following to ensure the Android build works.
#### Linux and OSX
Follow the setup steps for Linux or OSX above, then from your terminal:
```
cd build-android
./update_external_sources_android.sh
./android-generate.sh
ndk-build -j $(sysctl -n hw.ncpu)
```
#### Windows
Follow the setup steps for Windows above, then from Developer Command Prompt for VS2013:
```
cd build-android
update_external_sources_android.bat
android-generate.bat
ndk-build
```
#### Android demos
Use the following steps to build, install, and run Cube and Tri for Android:
```
cd demos/android
android update project -s -p . -t "android-23"
ndk-build
ant -buildfile cube debug
adb install ./cube/bin/NativeActivity-debug.apk
adb shell am start com.example.Cube/android.app.NativeActivity
```
To build, install, and run Cube with validation layers, first build layers using steps above, then run:
```
cd demos/android
android update project -s -p . -t "android-23"
ndk-build -j
ant -buildfile cube-with-layers debug
adb install ./cube-with-layers/bin/NativeActivity-debug.apk
adb shell am start -a android.intent.action.MAIN -c android-intent.category.LAUNCH -n com.example.CubeWithLayers/android.app.NativeActivity --es args "--validate"
```

To build, install, and run the Smoke demo for Android, run the following, and any
prompts that come back from the script:
```
./update_external_sources.sh --glslang
cd demos/smoke/android
export ANDROID_SDK_HOME=<path to Android/Sdk>
export ANDROID_NDK_HOME=<path to Android/Sdk/ndk-bundle>
./build-and-install
adb shell am start -a android.intent.action.MAIN -c android-intent.category.LAUNCH -n com.example.Smoke/android.app.NativeActivity --es args "--validate"
```

## Ninja Builds - All Platforms
The [Qt Creator IDE](https://qt.io/download-open-source/#section-2) can open a root CMakeList.txt as a project directly, and it provides tools within Creator to configure and generate Vulkan SDK build files for one to many targets concurrently, resolving configuration issues as needed. Alternatively, when invoking CMake use the -G Codeblocks Ninja option to generate Ninja build files to be used as project files for QtCreator

- Follow the steps defined elsewhere for the OS using the update\_external\_sources script or as shown in **Loader and Validation Layer Dependencies** below
- Open, configure, and build the gslang and spirv-tools CMakeList.txt files
- Then do the same with the Vulkan-LoaderAndValidationLayers CMakeList.txt file.
- In order to debug with QtCreator, a [Microsoft WDK: eg WDK 10](http://go.microsoft.com/fwlink/p/?LinkId=526733) is required. Note that installing the WDK breaks the MSVC vcvarsall.bat build scripts provided by MSVC, requiring that the LIB, INCLUDE, and PATH env variables be set to the WDK paths by some other means

## Loader and Validation Layer Dependencies
gslang and SPIRV-Tools repos are required to build and run Loader and Validation Layer components. They are not git sub-modules of Vulkan-LoaderAndValidationLayers but Vulkan-LoaderAndValidationLayers is linked to specific revisions of gslang and spirv-tools. These can be automatically cloned and built to predefined locations with the update\_external\_sources scripts. If a custom configuration is required, do the following steps:

1) clone the repos:

    git clone https://github.com/KhronosGroup/glslang.git
    git clone https://github.com/KhronosGroup/SPIRV-Tools.git


2) checkout the correct version of each tree based on the contents of the glslang\_revision and spirv-tools\_revision files at the root of the Vulkan-LoaderAndValidationLayers tree (do the same anytime that Vulkan-LoaderAndValidationLayers is updated from remote)

_on windows_

    git checkout < [path to Vulkan-LoaderAndValidationLayers]\glslang_revision [in glslang repo]
	git checkout < [path to Vulkan-LoaderAndValidationLayers]\spirv-tools_revision[in spriv-tools repo]

*non windows*

    git checkout `cat [path to Vulkan-LoaderAndValidationLayers]\glslang_revision` [in glslang repo]
	git checkout `cat [path to Vulkan-LoaderAndValidationLayers]\spirv-tools_revision` [in spriv-tools repo]

3) Configure the gslang and spirv-tools source trees with cmake and build them with your IDE of choice

4) Enable the CUSTOM\_GSLANG\_BIN\_PATH and CUSTOM\_SPIRV\_TOOLS\_BIN\_PATH options in the Vulkan-LoaderAndValidationLayers cmake configuration and point the GSLANG\_BINARY\_PATH and SPIRV\_TOOLS\_BINARY\_PATH variables to the correct location

5) If building on Windows with MSVC, set DISABLE\_BUILDTGT\_DIR\_DECORATION to _On_. If building on Windows, but without MSVC set DISABLE\_BUILD\_PATH\_DECORATION to _On_

## Optional software packages:

- [Cygwin for windows](https://www.cygwin.com/).  Notes:
  - Cygwin provides some Linux-like tools, which are valuable for obtaining the source code, and running CMake.
    Especially valuable are the BASH shell and git packages.
  - If you don't want to use Cygwin, there are other shells and environments that can be used.
    You can also use a Git package that doesn't come from Cygwin.

- [Ninja on all platforms](https://github.com/ninja-build/ninja/releases). [The Ninja-build project](ninja-build.org). [Ninja Users Manual](ninja-build.org/manual.html) 

- [QtCreator as IDE for CMake builds on all platforms](https://qt.io/download-open-source/#section-2)
