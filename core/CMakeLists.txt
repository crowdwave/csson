cmake_minimum_required(VERSION 3.28)
project(csson LANGUAGES C VERSION 1.0.0)

# --- C23 is MANDATORY (not best-effort), no GNU extensions -------------------
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)   # hard-require: never silently fall back to an older std
set(CMAKE_C_EXTENSIONS OFF)         # -std=c23 exactly (not gnu23)
# Fail configuration up front on a compiler that cannot provide C23.
if(NOT "c_std_23" IN_LIST CMAKE_C_COMPILE_FEATURES)
  message(FATAL_ERROR "CSSON requires a C23 compiler (gcc >= 14 or clang >= 18); "
                      "'${CMAKE_C_COMPILER}' does not advertise c_std_23.")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clang-tidy / clang-format / cppcheck

option(CSSON_SANITIZE  "Build with ASan + UBSan (test builds)"     OFF)
option(CSSON_TIDY      "Run clang-tidy on the project's C sources" OFF)

# Strict flags applied ONLY to our code (not vendored lexbor/yyjson).
set(CSSON_STRICT
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wstrict-prototypes -Werror)

# --- lexbor (trimmed: core + css + posix port), fetched, built as a SYSTEM dep
set(LEXBOR_DIR "${CMAKE_BINARY_DIR}/lexbor")
if(NOT EXISTS "${LEXBOR_DIR}/source")
  message(STATUS "Cloning lexbor…")
  execute_process(
    COMMAND bash -c "git clone --depth 1 --branch v3.1.0 https://github.com/lexbor/lexbor.git '${LEXBOR_DIR}' || git clone --depth 1 https://github.com/lexbor/lexbor.git '${LEXBOR_DIR}'"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0 OR NOT EXISTS "${LEXBOR_DIR}/source")
    message(FATAL_ERROR "lexbor clone failed")
  endif()
endif()
file(GLOB_RECURSE LEXBOR_SRC CONFIGURE_DEPENDS
  "${LEXBOR_DIR}/source/lexbor/core/*.c"
  "${LEXBOR_DIR}/source/lexbor/css/*.c"
  "${LEXBOR_DIR}/source/lexbor/ports/posix/*.c")
add_library(lexbor_csson STATIC ${LEXBOR_SRC})
target_include_directories(lexbor_csson SYSTEM PUBLIC "${LEXBOR_DIR}/source")
target_compile_options(lexbor_csson PRIVATE -Os -w)   # vendored: optimise for size, no warnings
set_target_properties(lexbor_csson PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)

# --- yyjson (vendored JSON parser for the RFC 6902 patch input) ---------------
add_library(yyjson STATIC third_party/yyjson.c)
target_include_directories(yyjson SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_options(yyjson PRIVATE -w)

# --- libcsson (the C ABI) -----------------------------------------------------
# Layered modules: foundation (mem/buffer/json/pointer) -> lexbor boundary
# (stylesheet) + leaf services (scalar/textedit/serialize) -> public ABI
# (read/edit/patch/version). See each header for its responsibility.
add_library(csson STATIC
  src/mem.c
  src/buffer.c
  src/json.c
  src/pointer.c
  src/stylesheet.c
  src/scalar.c
  src/textedit.c
  src/serialize.c
  src/read.c
  src/edit.c
  src/patch.c
  src/version.c)
target_include_directories(csson PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(csson PRIVATE _POSIX_C_SOURCE=200809L) # strdup under -std=c23
target_link_libraries(csson PUBLIC lexbor_csson PRIVATE yyjson m)
target_compile_options(csson PRIVATE ${CSSON_STRICT})

# --- csson CLI ----------------------------------------------------------------
add_executable(csson_cli cli/csson.c)
set_target_properties(csson_cli PROPERTIES OUTPUT_NAME csson)
target_link_libraries(csson_cli PRIVATE csson)
target_compile_options(csson_cli PRIVATE ${CSSON_STRICT})

# --- sanitizers (our targets) -------------------------------------------------
if(CSSON_SANITIZE)
  set(SAN -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_compile_options(csson PRIVATE ${SAN})
  target_compile_options(csson_cli PRIVATE ${SAN})
  target_link_options(csson_cli PRIVATE ${SAN})
endif()

# --- clang-tidy on our sources only -------------------------------------------
if(CSSON_TIDY)
  find_program(CLANG_TIDY clang-tidy REQUIRED)
  set_target_properties(csson csson_cli PROPERTIES C_CLANG_TIDY "${CLANG_TIDY}")
endif()

# --- install ------------------------------------------------------------------
install(TARGETS csson csson_cli
        ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
install(FILES include/csson.h DESTINATION include)

# --- tests: the CLI must reproduce the conformance canonical JSON -------------
enable_testing()
set(CONF "${CMAKE_CURRENT_SOURCE_DIR}/../conformance/v1")
add_test(NAME canon_v1 COMMAND ${CMAKE_COMMAND}
  -DCSSON=$<TARGET_FILE:csson_cli> -DDOC=${CONF}/csson_v1.csson -DEXP=${CONF}/expected.json
  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/canon_test.cmake)
add_test(NAME canon_numeric COMMAND ${CMAKE_COMMAND}
  -DCSSON=$<TARGET_FILE:csson_cli> -DDOC=${CONF}/fixtures/numeric.csson -DEXP=${CONF}/fixtures/numeric.expected.json
  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/canon_test.cmake)
add_test(NAME canon_edge COMMAND ${CMAKE_COMMAND}
  -DCSSON=$<TARGET_FILE:csson_cli> -DDOC=${CONF}/fixtures/edge.csson -DEXP=${CONF}/fixtures/edge.expected.json
  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/canon_test.cmake)

# Edit-path regression (locks the fixes for the edit/security bugs from the analysis).
add_test(NAME edits COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/edits.sh $<TARGET_FILE:csson_cli>)

# Security suite (docs/csson-security-analysis.md). Core-only checks (S1/S3/S5/S7
# + defended baseline); exits non-zero if any OPEN finding regresses. The browser
# parser-differential checks (S2/S4) run via conformance/v1/security with WITH_BROWSER=1.
add_test(NAME security COMMAND ${CMAKE_COMMAND} -E env CSSON=$<TARGET_FILE:csson_cli>
  bash ${CMAKE_CURRENT_SOURCE_DIR}/../conformance/v1/security/run.sh)
