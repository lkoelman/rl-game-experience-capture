## Question

Can I configure a GitHub repository to have a CI worfklow that builds an application for Windows 11, using the MSVC 2022 compiler toolchain?

## Gemini 3 Pro's Answer

Yes, absolutely. You can easily configure a GitHub Actions workflow to build an application for Windows 11 using the MSVC 2022 compiler.

GitHub provides Microsoft-hosted runners specifically for this purpose. By specifying the `windows-2022` (or `windows-latest`) environment, your workflow will run on a virtual machine that comes pre-installed with Visual Studio 2022 Enterprise, the MSVC v143 toolchain, and the latest Windows SDKs required to target Windows 11.

Here are the two most common ways to set this up, depending on your build system:

### Option 1: Using CMake (Recommended)
If your project uses CMake, it will automatically detect Visual Studio 2022 as the default generator on a `windows-2022` runner.

Create a file at `.github/workflows/build.yml` in your repository:

```yaml
name: Windows MSVC Build

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  build:
    runs-on: windows-2022

    steps:
    - name: Checkout code
      uses: actions/checkout@v4

    - name: Configure CMake
      # Generates project files using the VS 2022 generator for 64-bit architecture
      run: cmake -B ${{github.workspace}}/build -G "Visual Studio 17 2022" -A x64

    - name: Build
      # Builds the project using the MSVC compiler
      run: cmake --build ${{github.workspace}}/build --config Release
```

### Option 2: Using the Developer Command Prompt directly
If you are using Makefiles, custom build scripts, or want to invoke `cl.exe` or `MSBuild.exe` directly from the command line, you need to load the MSVC environment variables first. The easiest way to do this is by using a community action like `ilammy/msvc-dev-cmd`.

```yaml
name: Windows Command Line Build

on:
  push:
    branches: [ "main" ]

jobs:
  build:
    runs-on: windows-2022

    steps:
    - name: Checkout code
      uses: actions/checkout@v4

    - name: Setup MSVC Developer Command Prompt
      uses: ilammy/msvc-dev-cmd@v1
      with:
        arch: x64

    - name: Compile with cl.exe
      run: cl.exe /EHsc /O2 src\main.cpp /Fe:MyApp.exe

    - name: Run Application
      run: .\MyApp.exe
```

### Important Notes for Windows 11 Development:
* **Targeting Windows 11:** Building for "Windows 11" programmatically just means building with a recent Windows SDK. The `windows-2022` runner includes modern Windows SDKs (like 10.0.22621.0), which fully support Windows 11 APIs.
* **Backwards Compatibility:** By default, applications compiled with MSVC on these runners will run on both Windows 10 and Windows 11. If you need to restrict your app to Windows 11 exclusively, you will need to specify the minimum supported OS version in your application manifest or explicitly check the OS version at runtime in your code.
* **Artifacts:** If you want to download the compiled `.exe` file after the workflow finishes, you can add the `actions/upload-artifact@v4` step to upload your build directory.


## Additional Notes

The Github-hosted Windows runner images are described in https://github.com/actions/runner-images/tree/main/images/windows . It list the available images (each as a markdown file) along with the tools instaled in each image.