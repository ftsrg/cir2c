#
# Copyright 2025 Budapest University of Technology and Economics
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20)
project(cir2c LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# The oldest LLVM whose upstream ClangIR carries every CIR operation cir2c
# maps. See the "LLVM toolchain" section of README.md for the reason.
set(CIR2C_LLVM_MIN_VERSION 23.0)

# ---------------------------------------------------------------------------
# Locating the ClangIR-enabled LLVM/MLIR toolchain
# ---------------------------------------------------------------------------
# Resolution order (first hit wins):
#   1. -DLLVM_DIR=... / -DMLIR_DIR=...      explicit CMake package dirs
#   2. -DCIR2C_LLVM_PREFIX=... (or $CIR2C_LLVM_PREFIX)
#   3. <source dir>/llvm-install            docker/build-llvm.sh default for
#                                           in-tree builds
#   4. /opt/cir                             prefix inside the toolchain image
#   5. CMake's default search path          a system-wide LLVM install
# ---------------------------------------------------------------------------
if(NOT DEFINED CIR2C_LLVM_PREFIX AND DEFINED ENV{CIR2C_LLVM_PREFIX})
  set(CIR2C_LLVM_PREFIX "$ENV{CIR2C_LLVM_PREFIX}" CACHE PATH
      "Install prefix of a ClangIR-enabled LLVM/MLIR toolchain")
endif()

if(NOT DEFINED CIR2C_LLVM_PREFIX)
  foreach(candidate "${CMAKE_CURRENT_SOURCE_DIR}/llvm-install" "/opt/cir")
    if(EXISTS "${candidate}/lib/cmake/mlir")
      set(CIR2C_LLVM_PREFIX "${candidate}" CACHE PATH
          "Install prefix of a ClangIR-enabled LLVM/MLIR toolchain")
      break()
    endif()
  endforeach()
endif()

if(DEFINED CIR2C_LLVM_PREFIX)
  if(NOT DEFINED LLVM_DIR)
    set(LLVM_DIR "${CIR2C_LLVM_PREFIX}/lib/cmake/llvm")
  endif()
  if(NOT DEFINED MLIR_DIR)
    set(MLIR_DIR "${CIR2C_LLVM_PREFIX}/lib/cmake/mlir")
  endif()
endif()

if(DEFINED LLVM_DIR AND DEFINED MLIR_DIR)
  find_package(LLVM REQUIRED CONFIG PATHS "${LLVM_DIR}" NO_DEFAULT_PATH)
  find_package(MLIR REQUIRED CONFIG PATHS "${MLIR_DIR}" NO_DEFAULT_PATH)
else()
  # Nothing was pointed at explicitly — fall back to the system search path so
  # a distro-provided (or `module load`ed) LLVM works out of the box.
  find_package(LLVM REQUIRED CONFIG)
  find_package(MLIR REQUIRED CONFIG)
endif()

# Clang's package is optional: in an install prefix its headers already sit
# under LLVM_INCLUDE_DIRS, but in an LLVM *build tree* they are somewhere else
# entirely, and CLANG_INCLUDE_DIRS is the only thing that knows where.
if(NOT DEFINED Clang_DIR AND DEFINED LLVM_DIR)
  set(Clang_DIR "${LLVM_DIR}/../clang")
endif()
find_package(Clang CONFIG QUIET PATHS "${Clang_DIR}" NO_DEFAULT_PATH)

set(CIR2C_INCLUDE_DIRS ${LLVM_INCLUDE_DIRS} ${MLIR_INCLUDE_DIRS})
if(Clang_FOUND)
  list(APPEND CIR2C_INCLUDE_DIRS ${CLANG_INCLUDE_DIRS})
endif()
list(REMOVE_DUPLICATES CIR2C_INCLUDE_DIRS)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")
message(STATUS "Found MLIR (${MLIR_DIR})")

if(LLVM_PACKAGE_VERSION VERSION_LESS CIR2C_LLVM_MIN_VERSION)
  message(FATAL_ERROR
    "cir2c needs LLVM >= ${CIR2C_LLVM_MIN_VERSION} (found ${LLVM_PACKAGE_VERSION}).\n"
    "Earlier releases predate the CIR operations cir2c maps — LLVM 22 and older\n"
    "still model arithmetic as a single `cir.binop` instead of `cir.add`/`cir.sub`/…\n"
    "See the LLVM toolchain section of README.md.")
endif()

# The CIR dialect lives in clang's include tree, so an LLVM install built
# without ClangIR configures fine and then fails deep inside the first compile.
# Catch it here, where the message can say what to do about it. LLVM_INCLUDE_DIRS
# is a list (a build tree contributes both the source and the generated dir), so
# check each entry.
set(CIR2C_FOUND_CIR_HEADERS OFF)
foreach(incdir IN LISTS CIR2C_INCLUDE_DIRS)
  if(EXISTS "${incdir}/clang/CIR/Dialect/IR/CIRDialect.h")
    set(CIR2C_FOUND_CIR_HEADERS ON)
    break()
  endif()
endforeach()

if(NOT CIR2C_FOUND_CIR_HEADERS)
  message(FATAL_ERROR
    "This LLVM install has no ClangIR headers: clang/CIR/Dialect/IR/CIRDialect.h "
    "was not found under any of ${CIR2C_INCLUDE_DIRS}.\n"
    "cir2c needs an LLVM built with -DCLANG_ENABLE_CIR=ON; "
    "docker/build-llvm.sh produces one. See the Build section of README.md.")
endif()

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
include(AddLLVM)

# ---------------------------------------------------------------------------
# Git-based version string: <commit-hash>[-dirty]
# Regenerated on every build so it tracks working-tree changes.
# ---------------------------------------------------------------------------
find_package(Git QUIET)

set(VERSION_HEADER "${CMAKE_BINARY_DIR}/version.h")

add_custom_target(cir2c_version ALL
  BYPRODUCTS "${VERSION_HEADER}"
  COMMAND ${CMAKE_COMMAND}
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -DOUTPUT_FILE=${VERSION_HEADER}
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
  COMMENT "Generating version header"
)

add_executable(cir2c
  src/main.cpp
  src/Mapper.cpp
  src/ModuleEmitter.cpp
  src/ConstantEmitter.cpp
  src/TypeMapper.cpp
  src/Traceability.cpp
  src/handlers/Handlers.cpp
  src/handlers/HandlerModule.cpp
  src/handlers/HandlerSupport.cpp
  src/handlers/FloatMathHandlers.cpp
  src/handlers/ComplexHandlers.cpp
  src/handlers/BuiltinHandlers.cpp
  src/handlers/ArithmeticHandlers.cpp
  src/handlers/CastHandlers.cpp
  src/handlers/VariadicHandlers.cpp
  src/handlers/MemoryHandlers.cpp
  src/handlers/ConstantHandlers.cpp
  src/handlers/RecordHandlers.cpp
  src/handlers/GlobalHandlers.cpp
  src/handlers/CallHandlers.cpp
  src/handlers/ControlFlowHandlers.cpp
  src/handlers/VTableHandlers.cpp
  src/handlers/ExceptionHandlers.cpp
)

add_dependencies(cir2c cir2c_version)

target_include_directories(cir2c PRIVATE
  ${CIR2C_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src
  # Generated version header
  ${CMAKE_BINARY_DIR}
)
add_definitions(${LLVM_DEFINITIONS})

# Link to core MLIR libraries; additional dialect libs can be appended when needed.
target_link_libraries(cir2c PRIVATE
  MLIRIR
  MLIRParser
  MLIRSupport
  LLVMSupport
)

# cir2c parses CIR assembly rather than building it, so it needs the dialect
# registered at parse time but none of ClangIR's CodeGen. Link libMLIRCIR by
# absolute path so its static initializers run at startup and `cir.*` ops parse.
get_filename_component(LLVM_LIB_DIR "${LLVM_DIR}/../../" ABSOLUTE)
message(STATUS "Looking for MLIRCIR libraries in: ${LLVM_LIB_DIR}")

file(GLOB MLIRCIR_SHARED_LIBS CONFIGURE_DEPENDS "${LLVM_LIB_DIR}/libMLIRCIR.so*")
file(GLOB MLIRCIR_STATIC_LIBS CONFIGURE_DEPENDS "${LLVM_LIB_DIR}/libMLIRCIR.a")

if(MLIRCIR_SHARED_LIBS)
  message(STATUS "Found MLIRCIR shared libs: ${MLIRCIR_SHARED_LIBS}")
  target_link_libraries(cir2c PRIVATE ${MLIRCIR_SHARED_LIBS})
elseif(MLIRCIR_STATIC_LIBS)
  message(STATUS "Found MLIRCIR static libs: ${MLIRCIR_STATIC_LIBS}")

  # libMLIRCIR.a pulls in a web of mutually dependent MLIR/LLVM archives; the
  # link order that satisfies it is not worth maintaining by hand, so hand the
  # linker the whole directory inside a --start-group/--end-group pair.
  file(GLOB ALL_STATIC_LIBS CONFIGURE_DEPENDS "${LLVM_LIB_DIR}/*.a")
  list(LENGTH ALL_STATIC_LIBS NUM_STATIC_LIBS)
  message(STATUS "Linking all ${NUM_STATIC_LIBS} static libraries from ${LLVM_LIB_DIR}")

  if(NUM_STATIC_LIBS GREATER 0)
    target_link_libraries(cir2c PRIVATE
      -Wl,--start-group
      ${ALL_STATIC_LIBS}
      -Wl,--end-group
    )
  else()
    message(WARNING "No static libraries found in ${LLVM_LIB_DIR}, linking may fail")
    target_link_libraries(cir2c PRIVATE ${MLIRCIR_STATIC_LIBS})
  endif()
else()
  message(FATAL_ERROR
    "libMLIRCIR not found in ${LLVM_LIB_DIR}. Without it the CIR dialect is not "
    "registered and every `cir.*` operation fails to parse. Point cir2c at an "
    "LLVM built with -DCLANG_ENABLE_CIR=ON (see the Build section of README.md).")
endif()

install(TARGETS cir2c RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
# `ctest` runs the integration suite: every source under test/integration/input
# through the full clang → cir2c → clang pipeline. The larger corpora
# (LLVM SingleSource, esbmc-eval) are opt-in — see the Tests section of README.md.
enable_testing()

add_test(NAME version
  COMMAND $<TARGET_FILE:cir2c> --version
)

add_test(NAME integration
  COMMAND bash ${CMAKE_SOURCE_DIR}/test/run_tests.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(integration PROPERTIES
  ENVIRONMENT "CIR2C_BUILD_DIR=${CMAKE_BINARY_DIR};RUN_INTEGRATION=1;RUN_LLVM=0;RUN_ESBMC=0"
  TIMEOUT 1800
)
