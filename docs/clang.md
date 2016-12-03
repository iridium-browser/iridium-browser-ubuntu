# Clang

[Clang](http://clang.llvm.org/) is a compiler with many desirable features
(outlined on their website).

Chrome can be built with Clang. It is now the default compiler on Mac and Linux
for building Chrome, and it is currently useful for its warning and error
messages on Android and Windows.

See
[the open bugs](http://code.google.com/p/chromium/issues/list?q=label:clang).

[TOC]

## Build instructions

Get clang (happens automatically during `gclient runhooks` on Mac and Linux):

    tools/clang/scripts/update.py

Only needs to be run once per checkout, and clang will be automatically updated
by `gclient runhooks`.

Regenerate the ninja build files with Clang enabled. Again, on Linux and Mac,
Clang is the default compiler.

Run `gn args` and add `is_clang = true` to your args.gn file.

Build: `ninja -C out/Debug chrome`

## Reverting to gcc on linux

We don't have bots that test this, but building with gcc4.8+ should still work
on Linux. If your system gcc is new enough, run `gn args` and add `is_clang =
false`.

## Mailing List

http://groups.google.com/a/chromium.org/group/clang/topics

## Using plugins

The
[chromium style plugin](http://dev.chromium.org/developers/coding-style/chromium-style-checker-errors)
is used by default when clang is used.

If you're working on the plugin, you can build it locally like so:

1.  Run `./tools/clang/scripts/update.py --force-local-build --without-android`
    to build the plugin.
1.  Run `ninja -C third_party/llvm-build/Release+Asserts/` to build incrementally.
1.  Build with clang like described above, but, if you use goma, disable it.

To test the FindBadConstructs plugin, run:

    (cd tools/clang/plugins/tests && \
     ./test.py ../../../../third_party/llvm-build/Release+Asserts/bin/clang \
               ../../../../third_party/llvm-build/Release+Asserts/lib/libFindBadConstructs.so)

These instructions are for GYP which no longer works. Something similar needs
to be set up for the GN build if you want to do this. For reference, here are
the old instructions: To run [other plugins](writing_clang_plugins.md), add
these to your `GYP_DEFINES`:

*   `clang_load`: Absolute path to a dynamic library containing your plugin
*   `clang_add_plugin`: tells clang to run a specific PluginASTAction

So for example, you could use the plugin in this directory with:

*   `GYP_DEFINES='clang=1 clang_load=/path/to/libFindBadConstructs.so
    clang_add_plugin=find-bad-constructs' gclient runhooks`

## Using the clang static analyzer

See [clang_static_analyzer.md](clang_static_analyzer.md).

## Windows

clang can be used as compiler on Windows. Clang uses Visual Studio's linker and
SDK, so you still need to have Visual Studio installed.

Things should compile, and all tests should pass. You can check these bots for
how things are currently looking:
http://build.chromium.org/p/chromium.fyi/console?category=win%20clang

``` shell
python tools\clang\scripts\update.py
# run `gn args` and add `is_clang = true` to your args.gn, then...
ninja -C out\Debug chrome
```

The `update.py` script only needs to be run once per checkout. Clang will be
kept up to date by `gclient runhooks`.

Current brokenness:

*   Debug info is very limited.
*   To get colored diagnostics, you need to be running
    [ansicon](https://github.com/adoxa/ansicon/releases).

## Using a custom clang binary

These instructions are for GYP which no longer works. Something similar needs
to be set up for the GN build if you want to do this. For reference, here are
the old instructions:

If you want to try building Chromium with your own clang binary that you've
already built, set `make_clang_dir` to the directory containing `bin/clang`
(i.e. the directory you ran cmake in, or your `Release+Asserts` folder if you
use the configure/make build). You also need to disable chromium's clang plugin
by setting `clang_use_chrome_plugins=0`, as it likely won't load in your custom
clang binary.

Here's an example that also disables debug info and enables the component build
(both not strictly necessary, but they will speed up your build):

```shell
GYP_DEFINES="clang=1 fastbuild=1 component=shared_library \
clang_use_chrome_plugins=0 make_clang_dir=$HOME/src/llvm-build" \
build/gyp_chromium
```

You can then run `head out/Release/build.ninja` and check that the first to
lines set `cc` and `cxx` to your clang binary. If things look good, run `ninja
-C out/Release` to build.

If your clang revision is very different from the one currently used in chromium

*   Check `tools/clang/scripts/update.py` to find chromium's clang revision
*   You might have to tweak warning flags. Or you could set `werror=` in the
    line above to disable warnings as errors (but this only works on Linux).

## Using LLD

**Experimental!**

LLD is a relatively new linker from LLVM. The current focus is on Windows and
Linux support, where it can link Chrome approximately twice as fast as gold and
MSVC's link.exe as of this writing. LLD does not yet support generating PDB
files, which makes it hard to debug Chrome while using LLD.

Set `use_lld = true` in args.gn. Currently this configuration is only supported
on Windows.
