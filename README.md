LLVMCMakeBackend
====

An LLVM backend to CMake, you can compile C++/Rust into CMake now!

Well, this is a stupid project, but implementing it is a funny experience, and it is a challenge that implements LLVM IR in a restricted environment.

Building
----

This project depends on LLVM 13. So installing it first is needed, by `apt install llvm-13-dev` (add [llvm apt source](https://apt.llvm.org/) first) or building them yourself. Then run

```
mkdir build && cd build
cmake .. && cmake --build . --parallel
```

Usage
----

Run `./LLVMCMakeBackend Test.ll`, and if there are no errors, Test.ll.cmake will be generated. It can be run by `cmake -P Test.ll.cmake`, and can be included in some CMake script.

Hacking
----
The most of codes you may be interested in are located in lib folder. The codes in cli folder belong to the cli part, and not very valuable.
