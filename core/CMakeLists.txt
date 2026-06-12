cmake_minimum_required(VERSION 3.28)
project(csson LANGUAGES C VERSION 1.0.0)

# --- C23 mandatory (the facade), no GNU extensions ---------------------------
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(NOT "c_std_23" IN_LIST CMAKE_C_COMPILE_FEATURES)
  message(FATAL_ERROR "CSSON requires a C23 compiler (gcc >= 14 / clang >= 18).")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(CSSON_SANITIZE "Build with ASan + UBSan (test builds)" OFF)

set(CSSON_STRICT
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wstrict-prototypes -Werror)

# --- QuickJS-ng (fetched at a PINNED tag, built as a size-optimised SYSTEM dep)
# Pinned for reproducibility + CVE tracking: bump both together, never float.
set(QJS_TAG "v0.15.1")
set(QJS_SHA "fd0a0210b7be00957751871e7e01b8291268fc29")
set(QJS_DIR "${CMAKE_BINARY_DIR}/quickjs")
if(NOT EXISTS "${QJS_DIR}/quickjs.c")
  message(STATUS "Cloning quickjs-ng ${QJS_TAG}…")
  execute_process(
    COMMAND git clone --depth 1 --branch "${QJS_TAG}"
            https://github.com/quickjs-ng/quickjs.git "${QJS_DIR}"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0 OR NOT EXISTS "${QJS_DIR}/quickjs.c")
    message(FATAL_ERROR "quickjs-ng clone failed")
  endif()
endif()
# Verify the checkout is the exact pinned commit (guards against a moved tag).
execute_process(COMMAND git -C "${QJS_DIR}" rev-parse HEAD
                OUTPUT_VARIABLE QJS_HEAD OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT QJS_HEAD STREQUAL QJS_SHA)
  message(FATAL_ERROR
    "quickjs-ng pin mismatch: expected ${QJS_SHA}, got ${QJS_HEAD}.\n"
    "Delete ${QJS_DIR} to re-clone, or update QJS_TAG/QJS_SHA together.")
endif()
add_library(quickjs STATIC
  "${QJS_DIR}/quickjs.c" "${QJS_DIR}/libregexp.c" "${QJS_DIR}/libunicode.c" "${QJS_DIR}/dtoa.c")
target_include_directories(quickjs SYSTEM PUBLIC "${QJS_DIR}")
target_compile_options(quickjs PRIVATE -Os -w)
target_compile_definitions(quickjs PRIVATE _GNU_SOURCE)
# PIC so the static engine can be linked into the shared libcsson (FFI bindings).
set_target_properties(quickjs PROPERTIES C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)

# --- the JS bundle: our TypeScript + PostCSS + csstree -> a C byte array ------
# Build-time deps (package.json): installed automatically if node_modules is absent.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/node_modules")
  message(STATUS "npm install (CSSON JS build deps)…")
  execute_process(COMMAND npm install WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                  RESULT_VARIABLE npmrc)
  if(NOT npmrc EQUAL 0)
    message(FATAL_ERROR "npm install failed (node/npm required to build the bundle)")
  endif()
endif()
set(BUNDLE_H "${CMAKE_BINARY_DIR}/gen/csson_bundle.h")
add_custom_command(
  OUTPUT "${BUNDLE_H}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/gen"
  COMMAND node "${CMAKE_CURRENT_SOURCE_DIR}/tools/build-bundle.mjs" "${BUNDLE_H}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS ts/csson.ts tools/build-bundle.mjs
  COMMENT "Bundling TypeScript + PostCSS + csstree")
add_custom_target(csson_bundle DEPENDS "${BUNDLE_H}")

# --- libcsson: the C ABI facade over QuickJS + bundle ------------------------
add_library(csson STATIC src/facade.c)
add_dependencies(csson csson_bundle)
target_include_directories(csson
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_BINARY_DIR}/gen")
target_compile_definitions(csson PRIVATE _POSIX_C_SOURCE=200809L)
target_link_libraries(csson PUBLIC quickjs PRIVATE m)
target_compile_options(csson PRIVATE ${CSSON_STRICT})

# --- libcsson.so: SHARED build of the same ABI (for FFI bindings, e.g. Python) -
add_library(csson_shared SHARED src/facade.c)
add_dependencies(csson_shared csson_bundle)
set_target_properties(csson_shared PROPERTIES OUTPUT_NAME csson)
target_include_directories(csson_shared
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_BINARY_DIR}/gen")
target_compile_definitions(csson_shared PRIVATE _POSIX_C_SOURCE=200809L)
target_link_libraries(csson_shared PRIVATE quickjs m)
target_compile_options(csson_shared PRIVATE ${CSSON_STRICT})
install(TARGETS csson_shared LIBRARY DESTINATION lib)

# --- csson CLI ---------------------------------------------------------------
add_executable(csson_cli cli/csson.c)
set_target_properties(csson_cli PROPERTIES OUTPUT_NAME csson)
target_link_libraries(csson_cli PRIVATE csson)
target_compile_options(csson_cli PRIVATE ${CSSON_STRICT})

# --- handle-API test (C, links libcsson) -------------------------------------
add_executable(handle_test tests/handle_test.c)
target_link_libraries(handle_test PRIVATE csson)
target_compile_options(handle_test PRIVATE ${CSSON_STRICT})

if(CSSON_SANITIZE)
  set(SAN -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_compile_options(csson PRIVATE ${SAN})
  target_compile_options(csson_cli PRIVATE ${SAN})
  target_link_options(csson_cli PRIVATE ${SAN})
  target_compile_options(handle_test PRIVATE ${SAN})
  target_link_options(handle_test PRIVATE ${SAN})
endif()

install(TARGETS csson csson_cli ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
install(FILES include/csson.h DESTINATION include)

# --- tests: canonical JSON must match the conformance fixtures ----------------
enable_testing()
set(CONF "${CMAKE_CURRENT_SOURCE_DIR}/../conformance/v1")
add_test(NAME canon_v1 COMMAND ${CMAKE_COMMAND}
  -DCSSON=$<TARGET_FILE:csson_cli> -DDOC=${CONF}/csson_v1-csson.css -DEXP=${CONF}/expected.json
  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/canon_test.cmake)
add_test(NAME canon_numeric COMMAND ${CMAKE_COMMAND}
  -DCSSON=$<TARGET_FILE:csson_cli> -DDOC=${CONF}/fixtures/numeric-csson.css -DEXP=${CONF}/fixtures/numeric.expected.json
  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/canon_test.cmake)
add_test(NAME canon_edge COMMAND ${CMAKE_COMMAND}
  -DCSSON=$<TARGET_FILE:csson_cli> -DDOC=${CONF}/fixtures/edge-csson.css -DEXP=${CONF}/fixtures/edge.expected.json
  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/canon_test.cmake)
add_test(NAME canon_preprocess COMMAND ${CMAKE_COMMAND}
  -DCSSON=$<TARGET_FILE:csson_cli> -DDOC=${CONF}/fixtures/preprocess-csson.css -DEXP=${CONF}/fixtures/preprocess.expected.json
  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/canon_test.cmake)
add_test(NAME canon_selectors COMMAND ${CMAKE_COMMAND}
  -DCSSON=$<TARGET_FILE:csson_cli> -DDOC=${CONF}/fixtures/selectors-csson.css -DEXP=${CONF}/fixtures/selectors.expected.json
  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/canon_test.cmake)
add_test(NAME edits COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/edits.sh $<TARGET_FILE:csson_cli>)
add_test(NAME handle_api COMMAND handle_test)
# memcheck (quality-check cycle 5): drives every path incl. error/cleanup + a fuzz
# sweep. Bare here, so on a -DCSSON_SANITIZE=ON build it runs under ASan/UBSan/LSan;
# set MEMCHECK_VALGRIND=1 to wrap a non-sanitized binary under valgrind instead.
add_test(NAME memcheck COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/memcheck.sh $<TARGET_FILE:csson_cli>)
# security suite: the S1-S9 findings must stay closed (core-only here; set
# WITH_BROWSER=1 to also run the Chrome parser-differential checks).
add_test(NAME security COMMAND bash ${CONF}/security/run.sh)
set_tests_properties(security PROPERTIES
  ENVIRONMENT "CSSON=$<TARGET_FILE:csson_cli>" FAIL_REGULAR_EXPRESSION "still present: [1-9]")
