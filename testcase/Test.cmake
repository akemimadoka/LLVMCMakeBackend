# include(${CMAKE_CURRENT_LIST_DIR}/Test.ll.cmake)

# Main()
# message(STATUS "Main() returned ${_LLVM_CMAKE_RETURN_VALUE}")

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

LoopTest(4)
message(STATUS "LoopTest returns ${_LLVM_CMAKE_RETURN_VALUE}")

LoopTest2(4)
message(STATUS "LoopTest2 returns ${_LLVM_CMAKE_RETURN_VALUE}")

LoopTest3(4)
message(STATUS "LoopTest3 returns ${_LLVM_CMAKE_RETURN_VALUE}")

LoopTest4(4)
message(STATUS "LoopTest4 returns ${_LLVM_CMAKE_RETURN_VALUE}")

# include(${CMAKE_CURRENT_LIST_DIR}/VirtualTest.ll.cmake)

# set(Obj "")
# CreateVirtualBase(_LLVM_CMAKE_PTR.EXT:Obj)
# message(STATUS "CreateVirtualBase: ${Obj}")
# InvokeFunc(_LLVM_CMAKE_PTR.EXT:Obj 1)
# message(STATUS "InvokeFunc: ${_LLVM_CMAKE_RETURN_VALUE}")

# set(Obj "")
# CreateDerived(_LLVM_CMAKE_PTR.EXT:Obj)
# message(STATUS "CreateDerived: ${Obj}")
# InvokeFunc(_LLVM_CMAKE_PTR.EXT:Obj 1)
# message(STATUS "InvokeFunc: ${_LLVM_CMAKE_RETURN_VALUE}")

# include(${CMAKE_CURRENT_LIST_DIR}/TestRust.ll.cmake)
# _ZN8TestRust4mian17had6510f5de5bdbb1E()
# message(STATUS "mian: ${_LLVM_CMAKE_RETURN_VALUE}")
