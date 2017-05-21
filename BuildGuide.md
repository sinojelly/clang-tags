# Requirements
  - cmake
  - a C++11 compiler (e.g. g++ or clang++)
  - boost (at least the system and asio components)
  - jsoncpp
  - libclang (>= 3.0)
  - sqlite3
  - python (>= 2.3)
      - a version newer than 2.7 is recommended to benefit from the more recent argparse module.
The following are recommended:
  - git: to get the latest version
  - pkg-config: helps finding other requirements

# Building
clang-tags uses CMake as a build system.
Provided that the dependencies are installed (in standard locations), a complete build and test process should be as simple as:
cmake path/to/sources
make
make test

As an example, here is an installation procedure which has been tested on a fresh Debian Jessie system:
```
# Install dependencies
su -c "apt-get install build-essential git cmake pkg-config \
                       libboost-system-dev libboost-program-options-dev libjsoncpp-dev   \
                       libclang-dev libsqlite3-dev"

# Get clang-tags sources
mkdir clang-tags && cd clang-tags
git clone https://github.com/ffevotte/clang-tags.git src

# Create a build directory
mkdir build && cd build

# Configure the project
cmake    -DLIBCLANG_ROOT=/usr/lib/llvm-3.8 -DBOOST_ROOT=/usr/include/boost  -DCMAKE_BUILD_TYPE=Release  ../clang-tags
notes: replace the path with yours.

# Build and run tests
make
make test
```

# Testing and using without installing
It is possible to use clang-tags immediately after the build, without properly installing it on the system. Two environment setup scripts are generated during the build to help in this respect:
  - setup the shell environment: env.sh appropriately sets the required environment variables, such as PATH:
source env.sh

  - setup script for Emacs: env.el sets the required environment variables and loads the clang-tags.el library. You can either:
      - load it interactively: M-x load-file RET /path/to/build/env.el RET
      - or put the following in your Emacs initialization file (~/.emacs.d/init.el or ~/.emacs):
(load-file "/path/to/build/env.el")

Now would be a good time to proceed to the quick start guide or the user manual to ger started using clang-tags.

# Installing
Running make install in the build directory should install clang-tags, but this has not been tested much yet. For now, it is safer to follow the instructions in the previous section.
Emacs 23.3.1 (Org mode 8.0.6)
Validate XHTML 1.0

See also:
http://ffevotte.github.io/clang-tags/install.html

