cmake_minimum_required(VERSION 3.16)

project(mmiocpp VERSION 0.1.0 LANGUAGES CXX)

option(MMIOCPP_BUILD_DEMO "Build the host demo executable" ON)
option(MMIOCPP_BUILD_TESTS "Build the host compile-check executable" ON)
option(MMIOCPP_BUILD_QEMU_TARGET "Build the Cortex-M3 QEMU target runtime tests" OFF)
option(MMIOCPP_BUILD_QEMU_R5_TARGET "Build the Cortex-R5 QEMU target runtime tests" OFF)
option(MMIOCPP_ENABLE_COVERAGE "Build host executables with GCC/Clang coverage instrumentation" OFF)
option(MMIOCPP_ENABLE_TARGET_COVERAGE "Build QEMU target runtimes with GCC freestanding coverage instrumentation" OFF)
set(MMIOCPP_QEMU_SYSTEM_ARM "" CACHE FILEPATH "Path to qemu-system-arm.")
set(MMIOCPP_QEMU_MACHINE "mps2-an385" CACHE STRING "QEMU machine for the Cortex-M target.")
set(MMIOCPP_QEMU_CPU "cortex-m3" CACHE STRING "QEMU CPU for the Cortex-M target.")
set(MMIOCPP_QEMU_R5_MACHINE "none" CACHE STRING "QEMU machine for the Cortex-R5 target.")
set(MMIOCPP_QEMU_R5_CPU "cortex-r5" CACHE STRING "QEMU CPU for the Cortex-R5 target.")
set(MMIOCPP_QEMU_R5_MEMORY "32M" CACHE STRING "RAM size for the Cortex-R5 target.")
set(MMIOCPP_REPO_LOCAL_QEMU_SYSTEM_ARM "${CMAKE_CURRENT_SOURCE_DIR}/.local/qemu/bin/qemu-system-arm.exe")

function(mmiocpp_require_qemu_system_arm)
  if(NOT MMIOCPP_QEMU_SYSTEM_ARM AND DEFINED ENV{MMIOCPP_QEMU_SYSTEM_ARM} AND NOT "$ENV{MMIOCPP_QEMU_SYSTEM_ARM}" STREQUAL "")
    set(MMIOCPP_QEMU_SYSTEM_ARM "$ENV{MMIOCPP_QEMU_SYSTEM_ARM}" CACHE FILEPATH "Path to qemu-system-arm." FORCE)
  endif()

  if(NOT MMIOCPP_QEMU_SYSTEM_ARM AND EXISTS "${MMIOCPP_REPO_LOCAL_QEMU_SYSTEM_ARM}")
    set(MMIOCPP_QEMU_SYSTEM_ARM "${MMIOCPP_REPO_LOCAL_QEMU_SYSTEM_ARM}" CACHE FILEPATH "Path to qemu-system-arm." FORCE)
  endif()

  if(NOT MMIOCPP_QEMU_SYSTEM_ARM)
    message(FATAL_ERROR "qemu-system-arm was not found at ${MMIOCPP_REPO_LOCAL_QEMU_SYSTEM_ARM}. Run scripts/bootstrap.ps1 -InstallQemu or set MMIOCPP_QEMU_SYSTEM_ARM explicitly.")
  endif()

  if(NOT EXISTS "${MMIOCPP_QEMU_SYSTEM_ARM}")
    message(FATAL_ERROR "qemu-system-arm was configured as ${MMIOCPP_QEMU_SYSTEM_ARM}, but that file does not exist.")
  endif()
endfunction()
add_library(mmiocpp INTERFACE)
add_library(mmiocpp::mmiocpp ALIAS mmiocpp)

target_include_directories(mmiocpp
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(mmiocpp INTERFACE cxx_std_17)

function(mmiocpp_enable_coverage target_name)
  if(MMIOCPP_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
      target_link_options(${target_name} PRIVATE --coverage)
    else()
      message(WARNING "MMIOCPP_ENABLE_COVERAGE is only implemented for GNU and Clang host builds.")
    endif()
  endif()
endfunction()

function(mmiocpp_enable_target_coverage target_name)
  if(MMIOCPP_ENABLE_TARGET_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target_name} PRIVATE -O0 -g --coverage -fprofile-info-section -fprofile-abs-path)
      target_link_options(${target_name} PRIVATE --coverage)
      target_link_libraries(${target_name} PRIVATE gcov)
      target_compile_definitions(${target_name} PRIVATE MMIOCPP_TARGET_COVERAGE=1)
    else()
      message(WARNING "MMIOCPP_ENABLE_TARGET_COVERAGE is only implemented for GNU bare-metal builds.")
    endif()
  endif()
endfunction()

function(mmiocpp_copy_gnu_runtime target_name)
  if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_runtime_dlls
      libstdc++-6.dll
      libgcc_s_seh-1.dll
      libwinpthread-1.dll
    )

    foreach(_dll IN LISTS _runtime_dlls)
      find_file(_dll_path "${_dll}" PATHS "${_compiler_bin}" NO_DEFAULT_PATH)
      if(_dll_path)
        add_custom_command(TARGET "${target_name}" POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                  "${_dll_path}"
                  "$<TARGET_FILE_DIR:${target_name}>"
          VERBATIM
        )
      endif()
      unset(_dll_path CACHE)
    endforeach()
  endif()
endfunction()

if(MMIOCPP_BUILD_DEMO)
  add_executable(mmio_demo examples/mmio_demo.cpp)
  target_link_libraries(mmio_demo PRIVATE mmiocpp::mmiocpp)
  target_include_directories(mmio_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples)
  mmiocpp_enable_coverage(mmio_demo)
  mmiocpp_copy_gnu_runtime(mmio_demo)
endif()

if(MMIOCPP_BUILD_TESTS)
  enable_testing()

  add_executable(mmiocpp_tests tests/mmio_tests.cpp)
  target_link_libraries(mmiocpp_tests PRIVATE mmiocpp::mmiocpp)
  target_include_directories(mmiocpp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples)
  mmiocpp_enable_coverage(mmiocpp_tests)
  mmiocpp_copy_gnu_runtime(mmiocpp_tests)

  add_test(NAME mmiocpp_host_compile_checks COMMAND "$<TARGET_FILE_NAME:mmiocpp_tests>")
  set_tests_properties(mmiocpp_host_compile_checks PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )

  set(_mmiocpp_compile_fail_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_compile_fail_test.cmake")
  file(GLOB _mmiocpp_compile_fail_cases LIST_DIRECTORIES false CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail/*.cpp"
  )

  foreach(_mmiocpp_compile_fail_case IN LISTS _mmiocpp_compile_fail_cases)
    get_filename_component(_mmiocpp_compile_fail_name "${_mmiocpp_compile_fail_case}" NAME_WE)
    add_test(
      NAME "compile_fail_${_mmiocpp_compile_fail_name}"
      COMMAND "${CMAKE_COMMAND}"
        "-DMMIOCPP_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DMMIOCPP_SOURCE_FILE=${_mmiocpp_compile_fail_case}"
        "-DMMIOCPP_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DMMIOCPP_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${_mmiocpp_compile_fail_script}"
    )
  endforeach()
endif()

if(MMIOCPP_BUILD_QEMU_TARGET)
  enable_testing()
  enable_language(ASM)

  mmiocpp_require_qemu_system_arm()

  set(_mmiocpp_qemu_map_file "${CMAKE_CURRENT_BINARY_DIR}/mmiocpp_qemu_tests.map")

  add_executable(mmiocpp_qemu_tests
    targets/qemu-cortex-m/startup.S
    targets/qemu-cortex-m/runtime.cpp
    targets/qemu-cortex-m/mmio_target_tests.cpp
  )
  target_link_libraries(mmiocpp_qemu_tests PRIVATE mmiocpp::mmiocpp)
  target_include_directories(mmiocpp_qemu_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/examples
      ${CMAKE_CURRENT_SOURCE_DIR}/targets/qemu-cortex-m
  )
  target_compile_options(mmiocpp_qemu_tests
    PRIVATE
      -mcpu=cortex-m3
      -mthumb
      -ffreestanding
      -fno-exceptions
      -fno-rtti
      -fno-threadsafe-statics
      -fno-use-cxa-atexit
      -ffunction-sections
      -fdata-sections
  )
  target_link_options(mmiocpp_qemu_tests
    PRIVATE
      -mcpu=cortex-m3
      -mthumb
      -nostdlib
      -Wl,--gc-sections
      "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/targets/qemu-cortex-m/linker.ld"
      "-Wl,-Map,${_mmiocpp_qemu_map_file}"
  )
  set_target_properties(mmiocpp_qemu_tests PROPERTIES SUFFIX ".elf")

  if(MMIOCPP_QEMU_SYSTEM_ARM)
    add_custom_target(run_mmiocpp_qemu_tests
      COMMAND "${MMIOCPP_QEMU_SYSTEM_ARM}"
              -machine "${MMIOCPP_QEMU_MACHINE}"
              -cpu "${MMIOCPP_QEMU_CPU}"
              -nographic
              -monitor none
              -serial none
              -semihosting-config enable=on,target=native
              -kernel "$<TARGET_FILE_NAME:mmiocpp_qemu_tests>"
      DEPENDS mmiocpp_qemu_tests
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      USES_TERMINAL
      VERBATIM
    )

    add_test(NAME mmiocpp_qemu_runtime
      COMMAND "${MMIOCPP_QEMU_SYSTEM_ARM}"
              -machine "${MMIOCPP_QEMU_MACHINE}"
              -cpu "${MMIOCPP_QEMU_CPU}"
              -nographic
              -monitor none
              -serial none
              -semihosting-config enable=on,target=native
              -kernel "$<TARGET_FILE_NAME:mmiocpp_qemu_tests>"
    )
    set_tests_properties(mmiocpp_qemu_runtime PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
  endif()
endif()

if(MMIOCPP_BUILD_QEMU_R5_TARGET)
  enable_testing()
  enable_language(ASM)

  mmiocpp_require_qemu_system_arm()

  set(_mmiocpp_qemu_r5_map_file "${CMAKE_CURRENT_BINARY_DIR}/mmiocpp_qemu_r5_tests.map")

  add_executable(mmiocpp_qemu_r5_tests
    targets/qemu-cortex-r5/startup.S
    targets/qemu-cortex-r5/runtime.cpp
    targets/qemu-cortex-r5/mmio_target_tests.cpp
  )
  target_link_libraries(mmiocpp_qemu_r5_tests PRIVATE mmiocpp::mmiocpp)
  target_include_directories(mmiocpp_qemu_r5_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/examples
      ${CMAKE_CURRENT_SOURCE_DIR}/targets/qemu-cortex-r5
  )
  target_compile_options(mmiocpp_qemu_r5_tests
    PRIVATE
      -mcpu=cortex-r5
      -marm
      -ffreestanding
      -fno-exceptions
      -fno-rtti
      -fno-threadsafe-statics
      -fno-use-cxa-atexit
      -ffunction-sections
      -fdata-sections
  )
  target_link_options(mmiocpp_qemu_r5_tests
    PRIVATE
      -mcpu=cortex-r5
      -marm
      -nostdlib
      -Wl,--gc-sections
      "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/targets/qemu-cortex-r5/linker.ld"
      "-Wl,-Map,${_mmiocpp_qemu_r5_map_file}"
  )
  mmiocpp_enable_target_coverage(mmiocpp_qemu_r5_tests)
  if(MMIOCPP_ENABLE_TARGET_COVERAGE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
      targets/qemu-cortex-r5/runtime.cpp
      PROPERTIES COMPILE_OPTIONS "-fno-profile-arcs;-fno-test-coverage"
    )
  endif()
  set_target_properties(mmiocpp_qemu_r5_tests PROPERTIES SUFFIX ".elf")

  if(MMIOCPP_QEMU_SYSTEM_ARM)
    add_custom_target(run_mmiocpp_qemu_r5_tests
      COMMAND "${MMIOCPP_QEMU_SYSTEM_ARM}"
              -machine "${MMIOCPP_QEMU_R5_MACHINE}"
              -cpu "${MMIOCPP_QEMU_R5_CPU}"
              -m "${MMIOCPP_QEMU_R5_MEMORY}"
              -display none
              -monitor none
              -serial none
              -semihosting-config enable=on,target=native
              -device "loader,file=$<TARGET_FILE_NAME:mmiocpp_qemu_r5_tests>,cpu-num=0"
      DEPENDS mmiocpp_qemu_r5_tests
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      USES_TERMINAL
      VERBATIM
    )

    add_test(NAME mmiocpp_qemu_r5_runtime
      COMMAND "${MMIOCPP_QEMU_SYSTEM_ARM}"
              -machine "${MMIOCPP_QEMU_R5_MACHINE}"
              -cpu "${MMIOCPP_QEMU_R5_CPU}"
              -m "${MMIOCPP_QEMU_R5_MEMORY}"
              -display none
              -monitor none
              -serial none
              -semihosting-config enable=on,target=native
              -device "loader,file=$<TARGET_FILE_NAME:mmiocpp_qemu_r5_tests>,cpu-num=0"
    )
    set_tests_properties(mmiocpp_qemu_r5_runtime PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
  endif()
endif()
