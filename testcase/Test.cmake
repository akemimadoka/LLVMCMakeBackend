include(${CMAKE_CURRENT_LIST_DIR}/Test.ll.cmake)

Main()
message(STATUS "Main() returned ${_LLVM_CMAKE_RETURN_VALUE}")

# compile with `clang++ TestCpp.cpp -S -emit-llvm -o TestCpp.ll`
include(${CMAKE_CURRENT_LIST_DIR}/TestCpp.ll.cmake)

GetElem(1)
set(Ptr1 ${_LLVM_CMAKE_RETURN_VALUE})
message(STATUS "GetElem: ${Ptr1}")
LoadOffset(${Ptr1} 1)
message(STATUS "LoadOffset: ${_LLVM_CMAKE_RETURN_VALUE}")
GetElem(3)
set(Ptr2 ${_LLVM_CMAKE_RETURN_VALUE})
message(STATUS "GetElem: ${Ptr2}")
GetOffset(${Ptr1} ${Ptr2})
message(STATUS "GetOffset: ${_LLVM_CMAKE_RETURN_VALUE}")

CreatePair(1 2)
message(STATUS "returns: ${_LLVM_CMAKE_RETURN_VALUE}")

set(Dest ";")
CopyPair("_LLVM_CMAKE_PTR.EXT:Dest" "_LLVM_CMAKE_PTR.EXT:_LLVM_CMAKE_RETURN_VALUE")
message(STATUS "${Dest}")

TestInner0()
message(STATUS "${_LLVM_CMAKE_RETURN_VALUE}")

GetCallback(0)
InvokeFunc(${_LLVM_CMAKE_RETURN_VALUE} 1)
message(STATUS "InvokeFunc returns ${_LLVM_CMAKE_RETURN_VALUE}")

GetCallback(1)
InvokeFunc(${_LLVM_CMAKE_RETURN_VALUE} 1)
message(STATUS "InvokeFunc returns ${_LLVM_CMAKE_RETURN_VALUE}")

AsmTest(1 2)
message(STATUS "AsmTest returns ${_LLVM_CMAKE_RETURN_VALUE}")

LoopTest(4)
message(STATUS "LoopTest returns ${_LLVM_CMAKE_RETURN_VALUE}")

LoopTest2(4)
message(STATUS "LoopTest2 returns ${_LLVM_CMAKE_RETURN_VALUE}")

LoopTest3(4)
message(STATUS "LoopTest3 returns ${_LLVM_CMAKE_RETURN_VALUE}")

LoopTest4(4)
message(STATUS "LoopTest4 returns ${_LLVM_CMAKE_RETURN_VALUE}")

TestNew()
message(STATUS "TestNew returns ${_LLVM_CMAKE_RETURN_VALUE}, dereferenced: ${${_LLVM_CMAKE_RETURN_VALUE}}")

TestNew2()
message(STATUS "TestNew2 returns ${_LLVM_CMAKE_RETURN_VALUE}, dereferenced: ${${_LLVM_CMAKE_RETURN_VALUE}}")

# compile with `clang++ VirtualTest.cpp -S -emit-llvm -fno-rtti -fno-exceptions -o VirtualTest.ll`
include(${CMAKE_CURRENT_LIST_DIR}/VirtualTest.ll.cmake)

set(Obj "")
CreateVirtualBase(_LLVM_CMAKE_PTR.EXT:Obj)
message(STATUS "CreateVirtualBase: ${Obj}")
InvokeFunc(_LLVM_CMAKE_PTR.EXT:Obj 1)
message(STATUS "InvokeFunc: ${_LLVM_CMAKE_RETURN_VALUE}")

set(Obj "")
CreateDerived(_LLVM_CMAKE_PTR.EXT:Obj)
message(STATUS "CreateDerived: ${Obj}")
InvokeFunc(_LLVM_CMAKE_PTR.EXT:Obj 1)
message(STATUS "InvokeFunc: ${_LLVM_CMAKE_RETURN_VALUE}")

# compile with `rustc --emit=llvm-ir --crate-type=lib TestRust.rs -o TestRust.ll`
include(${CMAKE_CURRENT_LIST_DIR}/TestRust.ll.cmake)
rust_mian()
message(STATUS "rust_mian: ${_LLVM_CMAKE_RETURN_VALUE}")

# compile with `clang++ Fib.cpp -S -emit-llvm -o Fib.ll`
include(${CMAKE_CURRENT_LIST_DIR}/Fib.ll.cmake)
fib(100)
message(STATUS "fib: ${_LLVM_CMAKE_RETURN_VALUE}")
