# Building and tests

[← Back to the project](../README.md)

## Requirements

- Windows x64.
- **Visual Studio 2022** or **Build Tools for Visual Studio 2022** with **Desktop development with C++** installed.
- The **MSVC v143** toolset and a **Windows 10/11 SDK** from the 10.0 family.
- Git if you want to clone the source.

The compiled DLL requires **Microsoft Visual C++ 2015–2022 Redistributable x64**. It loads into a game server and is not a standalone executable.

## Build on Windows

Open **Developer Command Prompt for VS 2022**, clone the repository, and build:

```bat
git clone https://github.com/uytewey-dot/Vanta-bots.git
cd Vanta-bots
msbuild Magnesium.sln /m:2 /p:Configuration=Release /p:Platform=x64
```

The output is **`x64/Release/Magnesium.dll`**. You can also open `Magnesium.sln` in Visual Studio, select **Release | x64**, and choose **Build Solution**.

## Build with GitHub Actions

The [Build Vanta Bots workflow](../.github/workflows/build.yml) runs on pushes to `main` and on pull requests. It builds the Windows x64 DLL, runs regression tests, and uploads the DLL and PDB as **`Vanta-Bots-Windows-x64`**.

1. Open [Actions](https://github.com/uytewey-dot/Vanta-bots/actions/workflows/build.yml) and select **Build Vanta Bots**.
2. Open a successful run for the commit you want.
3. Under **Artifacts**, download `Vanta-Bots-Windows-x64` and extract it. Downloading workflow artifacts normally requires signing in to GitHub.

Artifacts appear after successful builds and remain available for the retention period configured in GitHub. If no runs are available, use a local build.

## Regression tests on Windows

Tests are separate executables and are not linked into the DLL. The complete commands are in the [workflow](../.github/workflows/build.yml). For example:

```bat
cl /nologo /std:c++20 /EHsc Magnesium\Tests\PlayerBotVersionSelectionTests.cpp /Fe:player-bot-version-selection-tests.exe
player-bot-version-selection-tests.exe
```

An exit code of `0` means the test passed.

## Portable tests on Linux

The profile and version-selection tests require a C++20-capable `g++`. They do not require Fortnite.

```sh
set -eu
vanta_test_dir="$(mktemp -d)"
for test in Fortnite3141ProfileTests FortniteShippingProfileTests Fortnite3211DecodeTests PlayerBotVersionSelectionTests; do
  g++ -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "Magnesium/Tests/$test.cpp" -o "$vanta_test_dir/$test"
  "$vanta_test_dir/$test"
done
```

`ArenaTelemetryWireTests` uses MSVC's `sprintf_s` formatting function and runs in Windows CI. Running it on Linux requires a standard-library compatibility adapter.

## Completed checks

The bot changes in commit `22b9d9d` were checked with:

- Compilation of 47 C++ translation units, including the PCH, resource compilation, and Windows x64 DLL linking with Clang/LLD 19, official MSVC v143 headers and libraries, and Windows SDK 10.0.26100.
- Regression tests for 28.30/31.41/32.11 profiles, 32.11 decoding, and version-selection restrictions with ASan/UBSan.
- The Arena telemetry test with a temporary Linux `sprintf_s` adapter.
- Parameter checks for eight game actions against each of the 12.41 and 14.60 SDKs.
- Version-card interactions, save events, mismatch restrictions, toggles, and 100%/150% interface scaling in a separate Dear ImGui test harness.

After the Vanta Bots branding update, the changed UI and initialization translation units were rebuilt and the DLL was relinked. Documentation translations do not change the binary.

These checks do not replace running the DLL in the corresponding game. See [COMPATIBILITY.md](COMPATIBILITY.md) for the limits of native compatibility verification.
