mkdir build
cd build
CXX=clang++ CC=clang cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja -DSIMPLE_LOGGER_TESTING=ON
ninja

