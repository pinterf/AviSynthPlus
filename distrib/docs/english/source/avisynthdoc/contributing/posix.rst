
Using AviSynth+ on POSIX systems
================================

As of version 3.5, AviSynth+ can now be built and used natively
on Linux, macOS, and BSD.

.. toctree::
    :maxdepth: 3

Related documents:

* :doc:`compiling_avsplus` — the general build guide for Windows, including
  cross-compiling Windows binaries from Linux.
* :doc:`avsplus_external_deps_guide_manual` — building DevIL and SoundTouch
  with Visual Studio.
* :doc:`avsplus_external_deps_guide_manual_arm` — Windows on ARM
  dependencies and toolchain, cross-compiled from Linux/WSL2.
* :doc:`release_build_automation` — the automated Windows dev build.
* :doc:`contributing_with_git` — git primer for contributors.

.. contents:: Table of contents



AviSynth+ prerequisites
-----------------------

Depending on your OS or distribution, the commands to fetch
the necessary prerequisites for building AviSynth+ differ.

At a bare minimum:

* CMake 3.8 or higher.
* GCC 8 or higher, or similarly recent version of Clang or AppleClang.

.. note::
   The use of Ninja as the generator for CMake is a matter of personal preference.
   Feel free to use GNU Make if so compelled (i.e. just a plain 'cmake ..' invocation).

Linux
^^^^^

Ubuntu 19.10 or higher
~~~~~~~~~~~~~~~~~~~~~~

::

    sudo apt-get install build-essential cmake git ninja-build checkinstall


::

    git clone https://github.com/AviSynth/AviSynthPlus && \
    cd AviSynthPlus && \
    mkdir avisynth-build && \
    cd avisynth-build && \

    cmake ../ -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    ninja && \
        sudo checkinstall --pkgname=avisynth --pkgversion="$(grep -r \
        Version avs_core/avisynth.pc | cut -f2 -d " ")-$(date --rfc-3339=date | \
        sed 's/-//g')-git" --backup=no --deldoc=yes --delspec=yes --deldesc=yes \
        --strip=yes --stripso=yes --addso=yes --fstrans=no --default ninja install


Ubuntu 18.04 LTS
~~~~~~~~~~~~~~~~

18.04 ships with GCC 7, which is not sufficient to build AviSynth+ without
the use of the `filesystem submodule`_.

::

    git clone --recursive https://github.com/AviSynth/AviSynthPlus && \
    cd AviSynthPlus && \
    mkdir avisynth-build && \
    cd avisynth-build && \

    cmake ../ -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    ninja && \
        sudo checkinstall --pkgname=avisynth --pkgversion="$(grep -r \
        Version avs_core/avisynth.pc | cut -f2 -d " ")-$(date --rfc-3339=date | \
        sed 's/-//g')-git" --backup=no --deldoc=yes --delspec=yes --deldesc=yes \
        --strip=yes --stripso=yes --addso=yes --fstrans=no --default ninja install

Raspbian Raspberry Pi 5 + llvm + ninja
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Raspberry Pi 5 is an aarch64 architecture, presently comes with a gcc 14.2.0 and Clang 19.1.7 
as of December 2025 (gcc was 12.2, Clang 14.0.6 in early 2025).

Version 12.2 was unfortunately unable to optimize well, namely it does not recognize well
the vectorizable code, and even produces slower code with a so-called "vector attribute"
that without it. Probably the ArmV8-a support is not perfect.

it seems that using llvm instead is the way to go.

This procedure was done on a trixie version:
::

    cat /etc/os-release

::

    PRETTY_NAME="Debian GNU/Linux 13 (trixie)"
    NAME="Debian GNU/Linux"
    VERSION_ID="13"
    VERSION="13 (trixie)"
    VERSION_CODENAME=trixie
    DEBIAN_VERSION_FULL=13.2
    ID=debian
    HOME_URL="https://www.debian.org/"
    SUPPORT_URL="https://www.debian.org/support"
    BUG_REPORT_URL="https://bugs.debian.org/"


I'm still using here the -DCMAKE_BUILD_TYPE=Release flag, but after a May 2025 source
it is no longer needed to avoid a completely unoptimized build with Ninja.

First, get some basic stuff.

::

    sudo apt-get install build-essential cmake git ninja-build checkinstall

::

    git clone https://github.com/AviSynth/AviSynthPlus && \
    cd AviSynthPlus && \
    mkdir avisynth-build && \
    cd avisynth-build

From now on, instead of a copy-pastable content, we create script files with the following contents.

Create a suitable directory for these scripts, and save them there.

*config.sh*: grabs the missing components for compiling AviSynth with llvm.
The prefix path is very important.
::

    #!/bin/bash

    # Configuration variables for LLVM build
    LLVM_PACKAGE="llvm"

    # --- AUTOMATIC VERSION DETECTION AND PATH SETUP ---
    # Detect the full path for CMake
    LLVM_CMAKE_PREFIX_PATH="$(llvm-config --prefix)"
    # Detect the major version for installing the specific compiler binary
    LLVM_MAJOR_VERSION=$(llvm-config --version | cut -d. -f1 2>/dev/null)

    if [ -z "$LLVM_MAJOR_VERSION" ]; then
        echo "Fatal Error: Could not determine LLVM major version. Cannot proceed."
        exit 1
    fi

    echo "Using LLVM CMake prefix path: $LLVM_CMAKE_PREFIX_PATH"

    # --- COMPLETE INSTALLATION CHECK ---
    # Check for the *specific* versioned compiler binary (e.g., clang-19)
    # AND the necessary libraries.
    if ! command -v "clang-$LLVM_MAJOR_VERSION" &> /dev/null || ! dpkg -s libc++-dev &> /dev/null || ! dpkg -s libc++abi-dev &> /dev/null; then
      echo "LLVM compiler (clang-$LLVM_MAJOR_VERSION) or required libraries not found. Installing..."
      sudo apt update

      # Install the specific versioned compiler package and libraries.
      # This fixes the problem where the generic 'llvm' package doesn't provide 'clang-19'.
      sudo apt install -y "clang-$LLVM_MAJOR_VERSION" libc++-dev libc++abi-dev
    fi


you can check the installed version and the prefix with
::

    llvm-config --version
    llvm-config --prefix

Or list your all clang versions
::

    ls /usr/bin/clang*

And query e.g. clang-19
::

    clang-19 --version
    
    Debian clang version 19.1.7 (3+b1)
    Target: aarch64-unknown-linux-gnu
    Thread model: posix
    InstalledDir: /usr/lib/llvm-19/bin

  
*build-llvm.sh* makes an actual clean build, assumes that avisynth was cloned
in the right folder in the previous step.

You can remove the general purge by remove the ``rm -rf *`` if everythings behaves as it should.

**build-llvm.sh**:
::

    #!/bin/bash

    # Source the configuration script (sets LLVM_CMAKE_PREFIX_PATH)
    source ./config.sh

    # 1. AUTOMATICALLY EXTRACT MAJOR LLVM VERSION
    # Uses the known working '--version' output and cuts out the major number (e.g., '19').
    LLVM_MAJOR_VERSION=$(llvm-config --version | cut -d. -f1 2>/dev/null)

    if [ -z "$LLVM_MAJOR_VERSION" ]; then
        echo "Fatal Error: Could not determine LLVM major version. Please run 'llvm-config --version' manually to verify installation."
        exit 1
    fi

    echo "Detected LLVM Major Version: $LLVM_MAJOR_VERSION"

    # Navigate to the build directory and clean up. Safely.
    BUILD_DIR=~/AviSynthPlus/avisynth-build
    # Navigate
    cd "$BUILD_DIR"
    # Clean up (The shell expands the unquoted * to all visible contents)
    rm -rf "$BUILD_DIR"/*

    # 2. RUN CMAKE WITH VERSIONED COMPILERS
    # Compilers are now set to the specific version (e.g., clang-19, clang++-19).
    cmake ../ -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="clang-$LLVM_MAJOR_VERSION" \
        -DCMAKE_CXX_COMPILER="clang++-$LLVM_MAJOR_VERSION" \
        -DCMAKE_PREFIX_PATH="$LLVM_CMAKE_PREFIX_PATH"

    # Check if CMake failed
    if [ $? -ne 0 ]; then
        echo "CMake configuration failed. Please check the output above."
        exit 1
    fi

    # 2. Build the project
    ninja

    # Check if ninja build failed
    if [ $? -ne 0 ]; then
        echo "Ninja build failed. Please check the output above."
        exit 1
    fi

    # 3. Prepare Package Version

    # The generated avisynth.pc is in the build directory, inside the avs_core folder.
    AVS_VERSION=$(cat avs_core/avisynth.pc | grep Version: | cut -d " " -f2)
    CURRENT_DATE=$(date --rfc-3339=date | sed 's/-//g')
    PACKAGE_VERSION="$AVS_VERSION-$CURRENT_DATE-git"

    echo "Package Version: $PACKAGE_VERSION"


    # 4.1 Ensure symlinks are (re)created during install so checkinstall includes them
    # Detect the SONAME from the freshly built real library (libavisynth.so.<version>)
    # and remove both the namelink (libavisynth.so) and the SONAME symlink (libavisynth.so.<SOVERSION>)
    # before running 'ninja install' under checkinstall. This guarantees they are recreated
    # during install and captured in the package payload.
    # Without this step, libavisynth.so may be missing after every second build, because
    # CMake reports existing symlinks as "Up-to-date", and checkinstall does not record
    # pre-existing filesystem items in the package payload.

    LIBDIR=/usr/lib/aarch64-linux-gnu

    # Find the real library we just built (e.g., libavisynth.so.3.7.5) in the target libdir
    REAL_LIB=$(ls -1 "$LIBDIR"/libavisynth.so.* 2>/dev/null | grep -E '\.so\.[0-9]+\.[0-9]+' | head -n1)

    # If the real library is not yet present in the target libdir (first install on a clean system),
    # try to read it from the build tree so we can still get the SONAME safely.
    if [ -z "$REAL_LIB" ]; then
      # Search in the build directory output (adjust if your library lands elsewhere during build)
      REAL_LIB=$(ls -1 "$BUILD_DIR"/libavisynth.so.* 2>/dev/null | grep -E '\.so\.[0-9]+\.[0-9]+' | head -n1)
    fi

    # If we managed to locate the real library, extract SONAME and remove symlinks safely.
    if [ -n "$REAL_LIB" ]; then
      SONAME=$(readelf -d "$REAL_LIB" 2>/dev/null | awk '/SONAME/ {print $5}' | tr -d '[]')
      if [ -n "$SONAME" ]; then
        echo "Detected SONAME: $SONAME (from $REAL_LIB)"
        # Remove namelink and SONAME symlink if present; leave the real file intact.
        sudo rm -f "$LIBDIR/libavisynth.so" "$LIBDIR/$SONAME"
      else
        echo "Warning: Could not detect SONAME via readelf; skipping pre-install symlink removal."
      fi
    else
      echo "Note: Real library not found before install; skipping pre-install symlink removal."
    fi

    # 4.2 Install using checkinstall
    sudo checkinstall --pkgname=avisynth \
        --pkgversion="$PACKAGE_VERSION" \
        --backup=no \
        --deldoc=yes \
        --delspec=yes \
        --deldesc=yes \
        --strip=yes \
        --stripso=yes \
        --addso=yes \
        --fstrans=no \
        --default \
        ninja install

    # 4.3 Refresh the dynamic linker cache
    sudo ldconfig

  
    # Navigate to the target bin directory
    cd ~/bin
  
    # 5. Success Message
    echo "AviSynth+ built and installed with LLVM version $LLVM_MAJOR_VERSION!"



When you are happy with gcc, use this script:

**build-gcc.sh**:
::

    #!/bin/bash

    # Navigate to the build directory and clean up. Safely.
    BUILD_DIR=~/AviSynthPlus/avisynth-build
    # Navigate
    cd "$BUILD_DIR"
    # Clean up (The shell expands the unquoted * to all visible contents)
    rm -rf "$BUILD_DIR"/*

    # ---  Get GCC Major Version ---
    GXX_MAJOR_VERSION=$(g++ -dumpversion | cut -d. -f1 2>/dev/null)

    if [ -z "$GXX_MAJOR_VERSION" ]; then
        echo "Warning: Could not determine G++ major version. Using \"$GXX_MAJOR_VERSION\"."
    fi
    # -----------------------------------

    # 1. Configure the build using the default system compiler (gcc/g++)
    cmake ../ -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

    # Check if CMake failed
    if [ $? -ne 0 ]; then
        echo "CMake configuration failed. Please check the output above."
        exit 1
    fi

    # 2. Build the project
    ninja

    # Check if ninja build failed
    if [ $? -ne 0 ]; then
        echo "Ninja build failed. Please check the output above."
        exit 1
    fi

    # 3. Prepare Package Version

    # The generated avisynth.pc is in the build directory, inside the avs_core folder.
    AVS_VERSION=$(cat avs_core/avisynth.pc | grep Version: | cut -d " " -f2)
    CURRENT_DATE=$(date --rfc-3339=date | sed 's/-//g')
    PACKAGE_VERSION="$AVS_VERSION-$CURRENT_DATE-git"

    echo "Package Version: $PACKAGE_VERSION"

    # 4.1 Ensure symlinks are (re)created during install so checkinstall includes them
    # Detect the SONAME from the freshly built real library (libavisynth.so.<version>)
    # and remove both the namelink (libavisynth.so) and the SONAME symlink (libavisynth.so.<SOVERSION>)
    # before running 'ninja install' under checkinstall. This guarantees they are recreated
    # during install and captured in the package payload.
    # Without this step, libavisynth.so may be missing after every second build, because
    # CMake reports existing symlinks as "Up-to-date", and checkinstall does not record
    # pre-existing filesystem items in the package payload.

    LIBDIR=/usr/lib/aarch64-linux-gnu

    # Find the real library we just built (e.g., libavisynth.so.3.7.5) in the target libdir
    REAL_LIB=$(ls -1 "$LIBDIR"/libavisynth.so.* 2>/dev/null | grep -E '\.so\.[0-9]+\.[0-9]+' | head -n1)

    # If the real library is not yet present in the target libdir (first install on a clean system),
    # try to read it from the build tree so we can still get the SONAME safely.
    if [ -z "$REAL_LIB" ]; then
      # Search in the build directory output (adjust if your library lands elsewhere during build)
      REAL_LIB=$(ls -1 "$BUILD_DIR"/libavisynth.so.* 2>/dev/null | grep -E '\.so\.[0-9]+\.[0-9]+' | head -n1)
    fi

    # If we managed to locate the real library, extract SONAME and remove symlinks safely.
    if [ -n "$REAL_LIB" ]; then
      SONAME=$(readelf -d "$REAL_LIB" 2>/dev/null | awk '/SONAME/ {print $5}' | tr -d '[]')
      if [ -n "$SONAME" ]; then
        echo "Detected SONAME: $SONAME (from $REAL_LIB)"
        # Remove namelink and SONAME symlink if present; leave the real file intact.
        sudo rm -f "$LIBDIR/libavisynth.so" "$LIBDIR/$SONAME"
      else
        echo "Warning: Could not detect SONAME via readelf; skipping pre-install symlink removal."
      fi
    else
      echo "Note: Real library not found before install; skipping pre-install symlink removal."
    fi

    # 4.2 Install using checkinstall
    sudo checkinstall --pkgname=avisynth \
        --pkgversion="$PACKAGE_VERSION" \
        --backup=no \
        --deldoc=yes \
        --delspec=yes \
        --deldesc=yes \
        --strip=yes \
        --stripso=yes \
        --addso=yes \
        --fstrans=no \
        --default \
        ninja install

    # 4.3 Refresh the dynamic linker cache
    sudo ldconfig
  
    # Navigate to the target bin directory
    cd ~/bin
  
    # 5. Success Message
    echo "AviSynth+ built and installed using GCC/G++ version $GXX_MAJOR_VERSION!"


For testing you can go on to the more advanced ffmpeg support.


Distributions without checkinstall
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Not all Linux distributions have checkinstall in their repositories, either due to
a lack of checkinstall working with their package management system or simply due
to omission.  In these cases, the install step is a little different:

::

    sudo ninja install
    sudo ldconfig


macOS
^^^^^

| Requires Homebrew:
| `<https://brew.sh/>`_

::

    brew install cmake ninja gcc


GCC isn't strictly necessary for AviSynth+, but it can side-step
the need to use `an external implementation`_ on High Sierra and
Mojave.


10.13 High Sierra and 10.14 Mojave
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Apple's libc++ doesn't support the C++17 filesystem functionality
on either of these versions of macOS, so we have to resort to
using `an external implementation`_ as a submodule.

::

    git clone --recursive https://github.com/AviSynth/AviSynthPlus && \
    cd AviSynthPlus && \
    mkdir avisynth-build && \
    cd avisynth-build

    cmake ../ -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    ninja && \
    sudo ninja install


10.15 Catalina and higher
~~~~~~~~~~~~~~~~~~~~~~~~~

C++17 filesystem support is available on Catalina, so it can
be built with the default Clang installation.

::

    cmake ../ -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    ninja && \
    sudo ninja install


FreeBSD
^^^^^^^

Tested on FreeBSD 12.1.

::

    pkg install cmake git gmake ninja

    git clone https://github.com/AviSynth/AviSynthPlus && \
    cd AviSynthPlus && \
    mkdir avisynth-build && \
    cd avisynth-build


Building AviSynth+ (GNU Make)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    cmake ../ && \
    gmake -j$(nproc) && \
    gmake install


Building AviSynth+ (Ninja)
~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    cmake ../ -G Ninja && \
    ninja && \
    sudo ninja install


FFmpeg support
--------------

On all of these OSes, AviSynth+ can interface with FFmpeg.
This change was applied to the FFmpeg git master branch on
2020-04-05.

It is always useful to check their guide before the process:
https://trac.ffmpeg.org/wiki/CompilationGuide


To compile a basic build of FFmpeg that supports
AviSynth+, the following steps will suffice:

Prerequisites
^^^^^^^^^^^^^

Linux
~~~~~


Ubuntu
......

First, enable the Sources repository by either enabling it
using the Software Sources dialog or by uncommenting the
right lines in /etc/apt/sources.list.

::

    sudo apt-get build-dep ffmpeg
    sudo apt-get install nasm libsdl2-dev


macOS
~~~~~

Homebrew prerequisites:

::

    brew install xz sdl2 pkg-config nasm


FreeBSD
~~~~~~~

::

    pkg install nasm sdl2


Building FFmpeg
^^^^^^^^^^^^^^^

::

    git clone https://git.videolan.org/git/ffmpeg.git
    cd ffmpeg


Linux
~~~~~

Ubuntu
......

::

    ./configure --prefix=$HOME/ffmpeg_build --enable-gpl --enable-version3 \
    --disable-doc --disable-debug --enable-pic --enable-avisynth && \
    make -j$(nproc) && \
    make install


Installing FFmpeg to the system can be done by leaving out the `--prefix`
option and then using the following checkinstall command:

::

    sudo checkinstall --pkgname=ffmpeg --pkgversion="7:$(git rev-list \
    --count HEAD)-g$(git rev-parse --short HEAD)" --backup=no --deldoc=yes \
    --delspec=yes --deldesc=yes --strip=yes --stripso=yes --addso=yes \
    --fstrans=no --default


Raspbian
........

Raspberry Pi 5 is able to compile ffmpeg and its prerequisites within a reasonable time, 
which means 7-10 minutes (when you don't forget setting -j4 for multithreaded build process).

This is a general Ubuntu/Debian compilation guide by the ffmpeg project:
https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu

You can also refer to this video (Raspberry Pi 5 - Compile FFMPEG on Raspberry PI OS):
https://www.youtube.com/live/if9UG4kJ9L4?si=Nq802tiUteRdlyHE

which helps you through the process.

Important: you'll need some additions, like extending the configuration with 
``--enable-avisynth`` and maybe ``--disable-doc``.

Here comes an all-in-one description:

First, enable the Sources repository by either enabling it
using the Software Sources dialog or by uncommenting the
right lines in /etc/apt/sources.list.

::

    sudo apt-get build-dep ffmpeg
    sudo apt-get install nasm libsdl2-dev

ffmpeg needs at least nasm 2.14, we have 2.16.03-1 on December 2025 Raspbian.
other prerequisites:

ffmpeg-pre.sh
::

    sudo apt-get update -qq && sudo apt-get -y install \
      autoconf \
      automake \
      build-essential \
      cmake \
      git-core \
      libass-dev \
      libfreetype6-dev \
      libgnutls28-dev \
      libmp3lame-dev \
      libsdl2-dev \
      libtool \
      libva-dev \
      libvdpau-dev \
      libvorbis-dev \
      libxcb1-dev \
      libxcb-shm0-dev \
      libxcb-xfixes0-dev \
      meson \
      ninja-build \
      pkg-config \
      texinfo \
      wget \
      yasm \
      zlib1g-dev

In your home directory make a new directory to put all of the source code and binaries into:

::

    mkdir -p ~/ffmpeg_sources ~/bin

Then various features, like libx264 
::

    sudo apt-get install libx264-dev

Finally FFmpeg build with avisynth, here I set only x264 and avisynth, a rather minimum setup to test.
::

    cd ~/ffmpeg_sources && \
    wget -O ffmpeg-snapshot.tar.bz2 https://ffmpeg.org/releases/ffmpeg-snapshot.tar.bz2 && \
    tar xjvf ffmpeg-snapshot.tar.bz2 && \
    cd ffmpeg && \
    PATH="$HOME/bin:$PATH" PKG_CONFIG_PATH="$HOME/ffmpeg_build/lib/pkgconfig" ./configure \
      --prefix="$HOME/ffmpeg_build" \
      --pkg-config-flags="--static" \
      --extra-cflags="-I$HOME/ffmpeg_build/include" \
      --extra-ldflags="-L$HOME/ffmpeg_build/lib" \
      --extra-libs="-lpthread -lm" \
      --ld="g++" \
      --bindir="$HOME/bin" \
      --enable-gpl \
      --enable-libx264 \
      --enable-avisynth && \
    PATH="$HOME/bin:$PATH" make -j4 && \
    make install && \
    hash -r


macOS
~~~~~

::

    ./configure --prefix=$HOME/ffmpeg_build --enable-gpl --enable-version3 --disable-doc \
    --disable-debug --enable-avisynth
    make -j$(nproc)
    make install

On Catalina, `--extra-cflags="-fno-stack-check"` is necessary when using AppleClang as the compiler.

FreeBSD
~~~~~~~

::

    ./configure --prefix=$HOME/ffmpeg_build --enable-gpl --enable-version3 --disable-doc \
    --disable-debug --enable-pic --enable-avisynth --cc=cc
    gmake -j$(nproc)
    gmake install


Testing the installation
------------------------

FFplay can be used to preview scripts in a pinch; if mpv or VLC is built against the patched
version of FFmpeg, those can be used to play back scripts in a more comfortable player
experience.

The easiest two scripts to test the installation are Version or Colorbars/ColorbarsHD.

::

    Version()


::

    Colorbars() # or ColorbarsHD()
    Info()


And running this script in the test build of FFmpeg:

::

    cd ~/ffmpeg_build/bin

or configure the PATH Permanently:
::

    nano ~/.bashrc
    
Make sure this line is near the top (or uncommented and correct):
::

    export PATH="$HOME/bin:$PATH"

Save and close the file, then apply the change:
::

    source ~/.bashrc

Now, which ffplay (or ffmpeg) should report /home/<yourhome>/bin/ffplay (/ffmpeg)


Create the script in a directory, for ease of testing.

To play the script::

    ffplay -i test.avs

To convert as usual::

    ffmpeg -i test.avs [encoding options]

Benchmark::

    ffmpeg -benchmark -i test.avs -f null -

Stream to a GUI, e.g. to VLC Media Player on a Raspberry Pi.
Choose "Open Network Stream" from menu, and enter ``udp://@127.0.0.1:10000`` 
for the network url.
::

    ffmpeg -i test.avs -pix_fmt yuv420p -f mpegts udp://127.0.0.1:10000


Troubleshooting:

- ffmpeg reports an unknown error. One possible reason is that libavisynth.so could
  not be loaded; the ``$LD_LIBRARY_PATH`` environment variable may be empty.
  Check with ``echo $LD_LIBRARY_PATH``.
  Fix it by (Raspberry Pi 5 Raspbian):

::

    echo 'export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH' >> ~/.bashrc

- ffmpeg does not recognize the AviSynth script. You probably typed ``ffmpeg`` 
  instead of ``./ffmpeg``. For example, Raspberry Pi 5 Raspbian comes with a 5.x ffmpeg 
  without AviSynth support, and your command finds that one.






Loading actual video sources will require a source filter.  FFMS2 doesn't require any porting
to these OSes, making it the most straightforward option at the moment.


Building FFMS2
--------------

FFMS2 doesn't require any additional prerequisites, so it can be
built straight away.

::

    git clone https://github.com/ffms/ffms2 && \
    cd ffms2


Linux
^^^^^

Ubuntu
~~~~~~

::

        PKG_CONFIG_PATH=$HOME/ffmpeg_build/lib/pkgconfig \
        CPPFLAGS="-I/usr/local/include/avisynth" \
        ./autogen.sh --enable-shared --enable-avisynth && \
    make -j$(nproc) && \
        sudo checkinstall --pkgname=ffms2 --pkgversion="1:$(./version.sh)-git" \
        --backup=no --deldoc=yes --delspec=yes --deldesc=yes --strip=yes --stripso=yes \
        --addso=yes --fstrans=no --default


macOS
^^^^^

::

    brew install autoconf automake libtool m4

        PKG_CONFIG_PATH=$HOME/ffmpeg_build/lib/pkgconfig \
        CPPFLAGS="-I/usr/local/include/avisynth" \
        ./autogen.sh --enable-shared --enable-avisynth && \
    make -j$(nproc) && \
    sudo make install


FreeBSD
^^^^^^^

::

    pkg install autoconf automake libtool m4

        PKG_CONFIG_PATH=$HOME/ffmpeg_build/lib/pkgconfig \
        CPPFLAGS="-I/usr/local/include/avisynth" \
        ./autogen.sh --enable-shared --enable-avisynth && \
    gmake -j$(nproc) && \
    gmake install


Plugin autoloading
------------------

AviSynth+ will use several directories for autoloading:
the `avisynth/` subdirectory where libavisynth.so was installed,
`$HOME/.avisynth`, and the directory given to the USER_AVS_PLUGINDIR_LOCATION
configuration option (defaults to `$HOME/.local/lib/avisynth`).
The latter of which can hold plugins (and symlinks to plugins)
or AVSI files without needing root permissions.

On FreeBSD, procfs needs to be mounted first in order for
autoloading to function.


Back to the :doc:`main page <../../index>`

$ Date: 2025-04-15 15:15:00 $

.. _an external implementation: https://github.com/gulrak/filesystem
.. _filesystem submodule: https://github.com/gulrak/filesystem
