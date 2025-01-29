# RL Game Experience Capture

# Building


```sh
conan install . --build=missing

cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="generators/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```