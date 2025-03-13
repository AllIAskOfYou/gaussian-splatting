# GaussianSplatting

## Building the project
Clone the repository and set it to cwd.

Parralel sorting requires TBB package to be installed.

* __Sequential sort__
```
cmake . -B build
```
* __Parralel sort__
```
cmake . -B build -DPARALLEL=ON -DTBB_PATH=<path-to-tbb-location>
```
(e.g. cmake . -B build -DPARALLEL=ON -DTBB_PATH="C:\msys64\mingw64\lib\cmake\TBB")

Then run the commands:
```
cmake --build build
build/App
```