LLVM_DIR=/Users/tom/workspace/llvm-main/build
export CXX=/usr/bin/clang++
cmake -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_PREFIX_PATH="$LLVM_DIR" -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Wunused-variable' ../
