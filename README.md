# cpp-core-systems
Low-level C++ runtime and systems foundations with performance-focused experiments.
This repository is used as a personal reference and learning codebase.

## Linux setup

Install a C++20-capable compiler, CMake 3.20 or newer, Git, and Make
(or Ninja if selected as your CMake generator). On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential cmake git
```

## Building on Linux

Build the library and examples with:

```bash
bash scripts/build.sh
```

The default is a Debug build in `out/build/linux-debug`. To build Release:

```bash
bash scripts/build.sh Release
```

Build configurations use separate directories, such as `out/build/linux-release`.
The script also supports `RelWithDebInfo` and `MinSizeRel`. Subsequent runs
rebuild changed files. You can invoke it by its absolute path from any directory.

## Running tests on Linux

From the repository root, run:

```bash
bash scripts/run-tests.sh
```

The script configures a Debug build in `out/build/linux-tests`, builds the
project, and runs all registered container and networking tests. Subsequent
runs reuse the build directory and rebuild changed files. The first run
downloads GoogleTest automatically. Any configuration, build, or test failure
returns a nonzero exit code.

You can also invoke the script by its absolute path from any directory.
Additional arguments go directly to CTest, for example:

```bash
bash scripts/run-tests.sh -L net
bash scripts/run-tests.sh -R SocketHandle
```
