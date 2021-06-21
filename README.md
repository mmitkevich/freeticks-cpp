# Freeticks: C++ algorithmic trading framework

## Prerequisites

To build Freeticks from source, you will need:

- [CMake >=3.13.2](http://www.cmake.org/) for Makefile generation;
- [GCC >=8.2.0](http://gcc.gnu.org/) or [Clang](http://clang.llvm.org/) with support for C++17;
- [Boost >=1.68.0](http://www.boost.org/) for additional library dependencies.
- [toolbox-cpp](http://github.com/mmitkevich/toolbox-cpp) for Reactive toolbox library
- [clickhouse-cpp] (http://github.com/clickhouse-cpp) for clickhouse sink
- [aeron] (https://github.com/real-logic/aeron) optionally for aeron transport
- [libpcap] 

## Getting Started

```bash
$ git clone git@github.com:mmitkevich/toolbox-cpp.git
$ git clone git@github.com:mmitkevich/freeticks-cpp.git
$ mkdir -p freeticks-cpp/build
$ cd freeticks-cpp && cmake -H. -Bbuild/relwithdebinfo -DCMAKE_BUILD_TYPE=relwithdebinfo \
                -DCMAKE_EXPORT_COMPILE_COMMANDS=YES \
                 -DBoost_NO_SYSTEM_PATHS=ON -DBOOST_ROOT=/opt/boost_1_68_0 \
                -DCMAKE_CXX_COMPILER=/opt/gcc-8.2.0/bin/g++ -DCMAKE_C_COMPILER=/opt/gcc-8.2.0/bin/gcc \
                -DCLICKHOUSECPP_ROOT_DIR=/opt/tbricks/local
                -DAERON_ROOT_DIR=/opt/aeron \                
```



### install instrument definitions
running requres instr-2020-11-02.xml definitions file from spbexchange ftp
```bash
# get 1Gb instrument definitions from FTP
wget http://ftp.spbexchange.ru/TS/instruments/instr-archive.zip
# extract instrument definitions xml
unzip -p instr-archive.zip instr-2020-11-02.xml > instr-2020-11-02.xml
```

## Running

### see run_sbp_mdserv.sh script for running details
```bash
LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH ./mdserv -l spb_mdserv.log -o spb_mdserv.out -v 5 -m pcap ./mdserv.json
```
## License

This project is licensed under the [Apache 2.0
License](https://www.apache.org/licenses/LICENSE-2.0). A copy of the license is available in the
[LICENSE.md](LICENSE.md) file in the root directory of the source tree.

