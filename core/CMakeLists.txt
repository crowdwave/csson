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

# --- QuickJS-ng (fetched, built as a size-optimised SYSTEM dep) ---------------
set(QJS_DIR "${CMAKE_BINARY_DIR}/quickjs")
if(NOT EXISTS "${QJS_DIR}/quickjs.c")
  message(STATUS "Cloning quickjs-ng…")
  execute_process(
    COMMAND bash -c "git clone --depth 1 https://github.com/quickjs-ng/quickjs.git '${QJS_DIR}'"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0 OR NOT EXISTS "${QJS_DIR}/quickjs.c")
    message(FATAL_ERROR "quickjs-ng clone failed")
  endif()
endif()
add_library(quickjs STATIC
  "${QJS_DIR}/quickjs.c" "${QJS_DIR}/libregexp.c" "${QJS_DIR}/libunicode.c" "${QJS_DIR}/dtoa.c")
target_include_directories(quickjs SYSTEM PUBLIC "${QJS_DIR}")
target_compile_options(quickjs PRIVATE -Os -w)
target_compile_definitions(quickjs PRIVATE _GNU_SOURCE)
set_target_properties(quickjs PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)

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

# --- csson CLI ---------------------------------------------------------------
add_executable(csson_cli cli/csson.c)
set_target_properties(csson_cli PROPERTIES OUTPUT_NAME csson)
target_link_libraries(csson_cli PRIVATE csson)
target_compile_options(csson_cli PRIVATE ${CSSON_STRICT})

if(CSSON_SANITIZE)
  set(SAN -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_compile_options(csson PRIVATE ${SAN})
  target_compile_options(csson_cli PRIVATE ${SAN})
  target_link_options(csson_cli PRIVATE ${SAN})
endif()

install(TARGETS csson csson_cli ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
install(FILES include/csson.h DESTINATION include)

# --- tests: canonical JSON must match the conformance fixtures ----------------
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
add_test(NAME edits COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/edits.sh $<TARGET_FILE:csson_cli>)
