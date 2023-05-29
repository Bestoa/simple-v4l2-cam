# Tiny camera

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4e8d02d2ad65472595d19da4366de0ad)](https://app.codacy.com/gh/Bestoa/simple-v4l2-cam/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)

Small tool based on V4L2.
### How to build:
```
cmake .
make
```
### How to cross build
```
cmake -DCMAKE_TOOLCHAIN_FILE=./CROSS-COMPILING.txt .
make
### Usage:
```
./tiny\_camera -h
```
