include(${CMAKE_CURRENT_LIST_DIR}/Test.ll.cmake)

Main()
message(STATUS "Main() returned ${_LLVM_CMAKE_Main_RETURN_VALUE}")

include(${CMAKE_CURRENT_LIST_DIR}/TestCpp.ll.cmake)
CreatePair(1 2)
message(STATUS "returns: ${_LLVM_CMAKE_CreatePair_RETURN_VALUE}")

set(Dest ";")
CopyPair("_LLVM_CMAKE_PTR.EXT:Dest" "_LLVM_CMAKE_PTR.EXT:_LLVM_CMAKE_CreatePair_RETURN_VALUE")
message(STATUS "${Dest}")

TestInner0()
message(STATUS "${_LLVM_CMAKE_TestInner0_RETURN_VALUE}")

GetCallback(0)
InvokeFunc(${_LLVM_CMAKE_GetCallback_RETURN_VALUE} 1)
message(STATUS "InvokeFunc returns ${_LLVM_CMAKE_InvokeFunc_RETURN_VALUE}")

GetCallback(1)
InvokeFunc(${_LLVM_CMAKE_GetCallback_RETURN_VALUE} 1)
message(STATUS "InvokeFunc returns ${_LLVM_CMAKE_InvokeFunc_RETURN_VALUE}")
