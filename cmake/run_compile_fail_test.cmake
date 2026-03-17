if(NOT DEFINED MMIOCPP_CXX_COMPILER)
  message(FATAL_ERROR "MMIOCPP_CXX_COMPILER is required.")
endif()

if(NOT DEFINED MMIOCPP_SOURCE_FILE)
  message(FATAL_ERROR "MMIOCPP_SOURCE_FILE is required.")
endif()

if(NOT DEFINED MMIOCPP_SOURCE_DIR)
  message(FATAL_ERROR "MMIOCPP_SOURCE_DIR is required.")
endif()

if(NOT DEFINED MMIOCPP_BINARY_DIR)
  message(FATAL_ERROR "MMIOCPP_BINARY_DIR is required.")
endif()

get_filename_component(_mmiocpp_case_name "${MMIOCPP_SOURCE_FILE}" NAME_WE)
set(_mmiocpp_output_dir "${MMIOCPP_BINARY_DIR}/compile_fail")
set(_mmiocpp_object_file "${_mmiocpp_output_dir}/${_mmiocpp_case_name}.o")
set(_mmiocpp_log_file "${_mmiocpp_output_dir}/${_mmiocpp_case_name}.log")

file(MAKE_DIRECTORY "${_mmiocpp_output_dir}")

execute_process(
  COMMAND "${MMIOCPP_CXX_COMPILER}"
          -std=c++17
          "-I${MMIOCPP_SOURCE_DIR}/include"
          "-I${MMIOCPP_SOURCE_DIR}/examples"
          -c "${MMIOCPP_SOURCE_FILE}"
          -o "${_mmiocpp_object_file}"
  RESULT_VARIABLE _mmiocpp_result
  OUTPUT_VARIABLE _mmiocpp_stdout
  ERROR_VARIABLE _mmiocpp_stderr
)

file(WRITE "${_mmiocpp_log_file}" "${_mmiocpp_stdout}${_mmiocpp_stderr}")

if(_mmiocpp_result EQUAL 0)
  file(REMOVE "${_mmiocpp_object_file}")
  message(FATAL_ERROR "Compile-fail test unexpectedly compiled: ${_mmiocpp_case_name}")
endif()