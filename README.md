# RL Game Experience Capture

# Building


```sh
conan install . --build=missing [-c tools.env.virtualenv:powershell=powershell.exe]

cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="generators/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```