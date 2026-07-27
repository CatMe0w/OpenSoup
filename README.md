# OpenSoup

## How to play

Grab the original Souptoys 1.6.0.8 installer from [here](https://www.majorgeeks.com/files/details/souptoys.html), [here](https://archive.org/details/Souptoys) or [here](https://souptoys.en.uptodown.com/).

Ensure the sha256sum of `souptoys-1.6.0.8.exe` is `f371c2f2e18be60e152fb5e45c2136e9c9b4e4a3efb4007509722354021689e2`.

Then, build OpenSoup by yourself because we don't have any stable release yet.

Run OpenSoup you just built, and it will tell you what to do next.

Enjoy your toys, just like 20 years ago.

## How to build

### Windows

CMake and [w64devkit](https://github.com/skeeto/w64devkit) (x86) is required. Note that x64 w64devkit is not supported.

Extract the w64devkit somewhere, and add its `bin` folder to your `PATH` environment variable.

Run the following commands:

```
cmake -B build
cmake --build build
```

Find the built executable in `build/opensoup.exe`.

### macOS and Linux

CMake is required. 

```
cmake -B build
cmake --build build
```

Find the built executable in `build/OpenSoup.app` (macOS) or `build/opensoup` (Linux).

> Linux is not supported yet. _Wayland bad bad_

## License

MIT License
