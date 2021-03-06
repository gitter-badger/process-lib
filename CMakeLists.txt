cmake_minimum_required(VERSION 3.1)
project(process-lib LANGUAGES C)

option(PROCESS_LIB_BUILD_TESTS "Build tests" OFF)
option(PROCESS_LIB_BUILD_EXAMPLES "Build examples" OFF)
option(PROCESS_LIB_BUILD_CPP_WRAPPER "Build C++ wrapper" OFF)
option(PROCESS_LIB_CI "Add -Werror or equivalent" OFF)

### Common Warnings ###

add_library(common-warnings INTERFACE)

if(MSVC)
  target_compile_options(common-warnings INTERFACE
    $<BUILD_INTERFACE:
      /W3
      $<$<BOOL:${PROCESS_LIB_CI}>:/WX>
    >
  )
else()
  target_compile_options(common-warnings INTERFACE
    $<BUILD_INTERFACE:
      -Wall
      -Wextra
      -pedantic-errors
      -Wshadow
      $<$<BOOL:${PROCESS_LIB_CI}>:-Werror>
    >
  )
endif()

### Library ###

add_library(process "")

set_property(TARGET process PROPERTY ARCHIVE_OUTPUT_DIRECTORY lib)

set_target_properties(process PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
)

target_link_libraries(process PRIVATE common-warnings)

target_include_directories(process PUBLIC include/c)

if(WIN32)
  target_include_directories(process PRIVATE src/c/windows)
  target_sources(process PRIVATE
    src/c/windows/handle.c
    src/c/windows/pipe.c
    src/c/windows/process_utils.c
    src/c/windows/process.c
    src/c/windows/string_utils.c
  )
elseif(UNIX)
  target_include_directories(process PRIVATE src/c/posix)
  target_sources(process PRIVATE
    src/c/posix/constants.c
    src/c/posix/fork_exec_redirect.c
    src/c/posix/pipe.c
    src/c/posix/process.c
    src/c/posix/wait.c
  )
endif()

if(MSVC)
  include(CheckSymbolExists)
  check_symbol_exists(InitializeProcThreadAttributeList windows.h
                      PROCESS_LIB_ATTRIBUTE_LIST_FOUND)
  target_compile_definitions(process PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
    $<$<BOOL:${PROCESS_LIB_ATTRIBUTE_LIST_FOUND}>:HAS_ATTRIBUTE_LIST>
  )
elseif(UNIX)
  include(CheckSymbolExists)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(pipe2 unistd.h PROCESS_LIB_PIPE2_FOUND)
  list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)

  target_compile_definitions(process PRIVATE
    _GNU_SOURCE
    $<$<BOOL:${PROCESS_LIB_PIPE2_FOUND}>:HAS_PIPE2>
  )
endif()

### C++ Wrapper ###

if(PROCESS_LIB_BUILD_CPP_WRAPPER)
  enable_language(CXX)

  target_compile_features(process PUBLIC cxx_std_11)
  set_target_properties(process PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )

  target_include_directories(process PUBLIC include/cpp)
  target_sources(process PRIVATE src/cpp/process.cpp)
endif()

### Tests ###

if(PROCESS_LIB_BUILD_TESTS)

  ### Helper programs ###

  enable_language(CXX)

  add_executable(echo test/res/echo.cpp)
  target_link_libraries(echo PRIVATE common-warnings)

  add_executable(infinite test/res/infinite.cpp)
  target_link_libraries(infinite PRIVATE common-warnings)

  add_executable(noop test/res/noop.cpp)
  target_link_libraries(noop PRIVATE common-warnings)

  set_property(TARGET echo infinite noop
    PROPERTY RUNTIME_OUTPUT_DIRECTORY test/res
  )

  ### Tests ###

  add_executable(tests "")

  add_dependencies(tests echo infinite noop)
  set_property(TARGET tests PROPERTY RUNTIME_OUTPUT_DIRECTORY test)

  target_link_libraries(tests PRIVATE common-warnings process)
  target_include_directories(tests SYSTEM PRIVATE third-party/doctest)

  target_compile_definitions(tests PRIVATE
    # Pass paths as compile time parameters so tests can be run from any
    # directory without hardcoding the path in the tests which differs for each
    # user.
    ECHO_PATH="$<TARGET_FILE:echo>"
    INFINITE_PATH="$<TARGET_FILE:infinite>"
    NOOP_PATH="$<TARGET_FILE:noop>"
    NOOP_DIR="$<TARGET_FILE_DIR:noop>"
  )

  target_sources(tests PRIVATE
    test/impl.cpp
    test/read-write.cpp
    test/stop.cpp
    test/working-directory.cpp
  )
endif()

### Examples ###

if(PROCESS_LIB_BUILD_EXAMPLES)
  add_executable(cmake-help-c examples/cmake-help.c)

  target_link_libraries(cmake-help-c PRIVATE common-warnings process)
  set_target_properties(cmake-help-c PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
  )

  add_executable(cmake-help-cpp examples/cmake-help.cpp)
  target_link_libraries(cmake-help-cpp common-warnings process)
  set_target_properties(cmake-help-cpp PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )

  set_property(TARGET cmake-help-c cmake-help-cpp
    PROPERTY RUNTIME_OUTPUT_DIRECTORY examples
  )
endif()

