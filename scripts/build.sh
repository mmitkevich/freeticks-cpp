DIR=$(dirname $0)
BUILD=relwithdebinfo
cd $DIR/../
export CXXFLAGS="" 

cmake -H. -Bbuild/$BUILD -DCMAKE_BUILD_TYPE=$BUILD \
                -DCMAKE_EXPORT_COMPILE_COMMANDS=YES \
                 -DBoost_NO_SYSTEM_PATHS=ON -DBOOST_ROOT=/opt/boost_1_68_0 \
                -DCMAKE_CXX_COMPILER=/opt/gcc-8.2.0/bin/g++ -DCMAKE_C_COMPILER=/opt/gcc-8.2.0/bin/gcc \
&& cd ./build/$BUILD && make -j6 #VERBOSE=1
#                -DCLICKHOUSECPP_ROOT_DIR=/opt/tbricks/local \
#                -DAERON_ROOT_DIR=/opt/aeron
#
