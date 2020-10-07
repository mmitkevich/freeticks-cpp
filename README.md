# Freeticks: C++ algorithmic trading framework

## What is Freeticks?

Freeticks is a collection of market connectivity components

## Prerequisites

To build Freeticks from source, you will need:

- [CMake](http://www.cmake.org/) for Makefile generation;
- [GCC](http://gcc.gnu.org/) or [Clang](http://clang.llvm.org/) with support for C++17;
- [Boost](http://www.boost.org/) for additional library dependencies.
- [toolbox-cpp](http://github.com/mmitkevich/toolbox-cpp) for Reactive toolbox library
## Getting Started

Clone the repository, build and run the unit-tests as follows:

```bash
$ git clone git@github.com:mmitkevich/toolbox-cpp.git
$ git clone git@github.com:mmitkevich/freeticks-cpp.git
$ mkdir freeticks-cpp/build
$ cd freeticks-cpp/build
$ cmake ..
$ make -j
```

## License

This project is licensed under the [Apache 2.0
License](https://www.apache.org/licenses/LICENSE-2.0). A copy of the license is available in the
[LICENSE.md](LICENSE.md) file in the root directory of the source tree.
