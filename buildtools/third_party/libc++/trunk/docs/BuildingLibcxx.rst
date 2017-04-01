
===============
Building libc++
===============

.. contents::
  :local:

Getting Started
===============

On Mac OS 10.7 (Lion) and later, the easiest way to get this library is to install
Xcode 4.2 or later.  However if you want to install tip-of-trunk from here
(getting the bleeding edge), read on.

The basic steps needed to build libc++ are:

#. Checkout LLVM:

   * ``cd where-you-want-llvm-to-live``
   * ``svn co http://llvm.org/svn/llvm-project/llvm/trunk llvm``

#. Checkout libc++:

   * ``cd where-you-want-llvm-to-live``
   * ``cd llvm/projects``
   * ``svn co http://llvm.org/svn/llvm-project/libcxx/trunk libcxx``

#. Checkout libc++abi:

   * ``cd where-you-want-llvm-to-live``
   * ``cd llvm/projects``
   * ``svn co http://llvm.org/svn/llvm-project/libcxxabi/trunk libcxxabi``

#. Configure and build libc++ with libc++abi:

   CMake is the only supported configuration system. Unlike other LLVM
   projects autotools is not supported for either libc++ or libc++abi.

   Clang is the preferred compiler when building and using libc++.

   * ``cd where you want to build llvm``
   * ``mkdir build``
   * ``cd build``
   * ``cmake -G <generator> [options] <path to llvm sources>``

   For more information about configuring libc++ see :ref:`CMake Options`.

   * ``make cxx`` --- will build libc++ and libc++abi.
   * ``make check-libcxx check-libcxxabi`` --- will run the test suites.

   Shared libraries for libc++ and libc++ abi should now be present in llvm/build/lib.
   See :ref:`using an alternate libc++ installation <alternate libcxx>`

#. **Optional**: Install libc++ and libc++abi

   If your system already provides a libc++ installation it is important to be
   careful not to replace it. Remember Use the CMake option ``CMAKE_INSTALL_PREFIX`` to
   select a safe place to install libc++.

   * ``make install-libcxx install-libcxxabi`` --- Will install the libraries and the headers

   .. warning::
     * Replacing your systems libc++ installation could render the system non-functional.
     * Mac OS X will not boot without a valid copy of ``libc++.1.dylib`` in ``/usr/lib``.


The instructions are for building libc++ on
FreeBSD, Linux, or Mac using `libc++abi`_ as the C++ ABI library.
On Linux, it is also possible to use :ref:`libsupc++ <libsupcxx>` or libcxxrt.

It is sometimes beneficial to build outside of the LLVM tree. An out-of-tree
build would look like this:

.. code-block:: bash

  $ cd where-you-want-libcxx-to-live
  $ # Check out llvm, libc++ and libc++abi.
  $ ``svn co http://llvm.org/svn/llvm-project/llvm/trunk llvm``
  $ ``svn co http://llvm.org/svn/llvm-project/libcxx/trunk libcxx``
  $ ``svn co http://llvm.org/svn/llvm-project/libcxxabi/trunk libcxxabi``
  $ cd where-you-want-to-build
  $ mkdir build && cd build
  $ export CC=clang CXX=clang++
  $ cmake -DLLVM_PATH=path/to/llvm \
          -DLIBCXX_CXX_ABI=libcxxabi \
          -DLIBCXX_CXX_ABI_INCLUDE_PATHS=path/to/libcxxabi/include \
          path/to/libcxx
  $ make
  $ make check-libcxx # optional


.. _`libc++abi`: http://libcxxabi.llvm.org/


.. _CMake Options:

CMake Options
=============

Here are some of the CMake variables that are used often, along with a
brief explanation and LLVM-specific notes. For full documentation, check the
CMake docs or execute ``cmake --help-variable VARIABLE_NAME``.

**CMAKE_BUILD_TYPE**:STRING
  Sets the build type for ``make`` based generators. Possible values are
  Release, Debug, RelWithDebInfo and MinSizeRel. On systems like Visual Studio
  the user sets the build type with the IDE settings.

**CMAKE_INSTALL_PREFIX**:PATH
  Path where LLVM will be installed if "make install" is invoked or the
  "INSTALL" target is built.

**CMAKE_CXX_COMPILER**:STRING
  The C++ compiler to use when building and testing libc++.


.. _libcxx-specific options:

libc++ specific options
-----------------------

.. option:: LIBCXX_ENABLE_ASSERTIONS:BOOL

  **Default**: ``ON``

  Build libc++ with assertions enabled.

.. option:: LIBCXX_BUILD_32_BITS:BOOL

  **Default**: ``OFF``

  Build libc++ as a 32 bit library. Also see :option:`LLVM_BUILD_32_BITS`.

.. option:: LIBCXX_ENABLE_SHARED:BOOL

  **Default**: ``ON``

  Build libc++ as a shared library. If ``OFF`` is specified then libc++ is
  built as a static library.

.. option:: LIBCXX_LIBDIR_SUFFIX:STRING

  Extra suffix to append to the directory where libraries are to be installed.
  This option overrides :option:`LLVM_LIBDIR_SUFFIX`.

.. _ABI Library Specific Options:

ABI Library Specific Options
----------------------------

.. option:: LIBCXX_CXX_ABI:STRING

  **Values**: ``none``, ``libcxxabi``, ``libcxxrt``, ``libstdc++``, ``libsupc++``.

  Select the ABI library to build libc++ against.

.. option:: LIBCXX_CXX_ABI_INCLUDE_PATHS:PATHS

  Provide additional search paths for the ABI library headers.

.. option:: LIBCXX_CXX_ABI_LIBRARY_PATH:PATH

  Provide the path to the ABI library that libc++ should link against.

.. option:: LIBCXX_ENABLE_STATIC_ABI_LIBRARY:BOOL

  **Default**: ``OFF``

  If this option is enabled, libc++ will try and link the selected ABI library
  statically.

.. option:: LIBCXX_ENABLE_ABI_LINKER_SCRIPT:BOOL

  **Default**: ``ON`` by default on UNIX platforms other than Apple unless
  'LIBCXX_ENABLE_STATIC_ABI_LIBRARY' is ON. Otherwise the default value is ``OFF``.

  This option generate and installs a linker script as ``libc++.so`` which
  links the correct ABI library.

.. option:: LIBCXXABI_USE_LLVM_UNWINDER:BOOL

  **Default**: ``OFF``

  Build and use the LLVM unwinder. Note: This option can only be used when
  libc++abi is the C++ ABI library used.


libc++ Feature options
----------------------

.. option:: LIBCXX_ENABLE_EXCEPTIONS:BOOL

  **Default**: ``ON``

  Build libc++ with exception support.

.. option:: LIBCXX_ENABLE_RTTI:BOOL

  **Default**: ``ON``

  Build libc++ with run time type information.


libc++ Feature options
----------------------

The following options allow building libc++ for a different ABI version.

.. option:: LIBCXX_ABI_VERSION:STRING

  **Default**: ``1``

  Defines the target ABI version of libc++.

.. option:: LIBCXX_ABI_UNSTABLE:BOOL

  **Default**: ``OFF``

  Build the "unstable" ABI version of libc++. Includes all ABI changing features
  on top of the current stable version.

.. _LLVM-specific variables:

LLVM-specific options
---------------------

.. option:: LLVM_LIBDIR_SUFFIX:STRING

  Extra suffix to append to the directory where libraries are to be
  installed. On a 64-bit architecture, one could use ``-DLLVM_LIBDIR_SUFFIX=64``
  to install libraries to ``/usr/lib64``.

.. option:: LLVM_BUILD_32_BITS:BOOL

  Build 32-bits executables and libraries on 64-bits systems. This option is
  available only on some 64-bits unix systems. Defaults to OFF.

.. option:: LLVM_LIT_ARGS:STRING

  Arguments given to lit.  ``make check`` and ``make clang-test`` are affected.
  By default, ``'-sv --no-progress-bar'`` on Visual C++ and Xcode, ``'-sv'`` on
  others.


Using Alternate ABI libraries
=============================


.. _libsupcxx:

Using libsupc++ on Linux
------------------------

You will need libstdc++ in order to provide libsupc++.

Figure out where the libsupc++ headers are on your system. On Ubuntu this
is ``/usr/include/c++/<version>`` and ``/usr/include/c++/<version>/<target-triple>``

You can also figure this out by running

.. code-block:: bash

  $ echo | g++ -Wp,-v -x c++ - -fsyntax-only
  ignoring nonexistent directory "/usr/local/include/x86_64-linux-gnu"
  ignoring nonexistent directory "/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../x86_64-linux-gnu/include"
  #include "..." search starts here:
  #include &lt;...&gt; search starts here:
  /usr/include/c++/4.7
  /usr/include/c++/4.7/x86_64-linux-gnu
  /usr/include/c++/4.7/backward
  /usr/lib/gcc/x86_64-linux-gnu/4.7/include
  /usr/local/include
  /usr/lib/gcc/x86_64-linux-gnu/4.7/include-fixed
  /usr/include/x86_64-linux-gnu
  /usr/include
  End of search list.

Note that the first two entries happen to be what we are looking for. This
may not be correct on other platforms.

We can now run CMake:

.. code-block:: bash

  $ CC=clang CXX=clang++ cmake -G "Unix Makefiles" \
    -DLIBCXX_CXX_ABI=libstdc++ \
    -DLIBCXX_CXX_ABI_INCLUDE_PATHS="/usr/include/c++/4.7/;/usr/include/c++/4.7/x86_64-linux-gnu/" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    <libc++-source-dir>


You can also substitute ``-DLIBCXX_CXX_ABI=libsupc++``
above, which will cause the library to be linked to libsupc++ instead
of libstdc++, but this is only recommended if you know that you will
never need to link against libstdc++ in the same executable as libc++.
GCC ships libsupc++ separately but only as a static library.  If a
program also needs to link against libstdc++, it will provide its
own copy of libsupc++ and this can lead to subtle problems.

.. code-block:: bash

  $ make cxx
  $ make install

You can now run clang with -stdlib=libc++.
