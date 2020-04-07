include(${CMAKE_CURRENT_LIST_DIR}/Test.ll.cmake)

Main()
message(STATUS "Main() returned ${_LLVM_CMAKE_Main_RETURN_VALUE}")

include(${CMAKE_CURRENT_LIST_DIR}/TestCpp.ll.cmake)
CreatePair(1 2)
message(STATUS "${_LLVM_CMAKE_CreatePair_RETURN_VALUE}")

set(Dest ";")
CopyPair(Dest _LLVM_CMAKE_CreatePair_RETURN_VALUE)
message(STATUS "${Dest}")
