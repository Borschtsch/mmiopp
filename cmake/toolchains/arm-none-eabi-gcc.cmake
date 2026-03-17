set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_mmiocpp_arm_gcc_hint "$ENV{MMIOCPP_ARM_GCC}")
set(_mmiocpp_arm_gxx_hint "$ENV{MMIOCPP_ARM_GXX}")

if(WIN32)
  file(GLOB _mmiocpp_arm_gcc_candidates
    "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/*/bin/arm-none-eabi-gcc.exe"
  )
  file(GLOB _mmiocpp_arm_gxx_candidates
    "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/*/bin/arm-none-eabi-g++.exe"
  )

  if(_mmiocpp_arm_gcc_candidates)
    list(SORT _mmiocpp_arm_gcc_candidates COMPARE NATURAL ORDER DESCENDING)
    list(GET _mmiocpp_arm_gcc_candidates 0 _mmiocpp_arm_gcc_default)
  endif()

  if(_mmiocpp_arm_gxx_candidates)
    list(SORT _mmiocpp_arm_gxx_candidates COMPARE NATURAL ORDER DESCENDING)
    list(GET _mmiocpp_arm_gxx_candidates 0 _mmiocpp_arm_gxx_default)
  endif()
endif()

if(_mmiocpp_arm_gcc_hint)
  set(MMIOCPP_ARM_GCC "${_mmiocpp_arm_gcc_hint}" CACHE FILEPATH "Path to arm-none-eabi-gcc." FORCE)
elseif(_mmiocpp_arm_gcc_default)
  set(MMIOCPP_ARM_GCC "${_mmiocpp_arm_gcc_default}" CACHE FILEPATH "Path to arm-none-eabi-gcc." FORCE)
else()
  find_program(MMIOCPP_ARM_GCC
    NAMES arm-none-eabi-gcc arm-none-eabi-gcc.exe
    HINTS
      "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi"
    PATH_SUFFIXES bin
    DOC "Path to arm-none-eabi-gcc."
  )
endif()

if(_mmiocpp_arm_gxx_hint)
  set(MMIOCPP_ARM_GXX "${_mmiocpp_arm_gxx_hint}" CACHE FILEPATH "Path to arm-none-eabi-g++.exe" FORCE)
elseif(_mmiocpp_arm_gxx_default)
  set(MMIOCPP_ARM_GXX "${_mmiocpp_arm_gxx_default}" CACHE FILEPATH "Path to arm-none-eabi-g++.exe" FORCE)
else()
  find_program(MMIOCPP_ARM_GXX
    NAMES arm-none-eabi-g++ arm-none-eabi-g++.exe
    HINTS
      "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi"
    PATH_SUFFIXES bin
    DOC "Path to arm-none-eabi-g++."
  )
endif()

if(NOT MMIOCPP_ARM_GCC)
  message(FATAL_ERROR "arm-none-eabi-gcc was not found. Set MMIOCPP_ARM_GCC or install the Arm GNU Toolchain.")
endif()

if(NOT MMIOCPP_ARM_GXX)
  message(FATAL_ERROR "arm-none-eabi-g++ was not found. Set MMIOCPP_ARM_GXX or install the Arm GNU Toolchain.")
endif()

set(CMAKE_C_COMPILER "${MMIOCPP_ARM_GCC}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${MMIOCPP_ARM_GXX}" CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER "${MMIOCPP_ARM_GCC}" CACHE FILEPATH "" FORCE)

get_filename_component(_mmiocpp_arm_bin_dir "${MMIOCPP_ARM_GCC}" DIRECTORY)
get_filename_component(_mmiocpp_arm_root_dir "${_mmiocpp_arm_bin_dir}" DIRECTORY)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")
set(CMAKE_FIND_ROOT_PATH "${_mmiocpp_arm_root_dir}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)