
# The CMakeLists of libqasm require SWIG and a recent Python dev version,
# because the Python build/link steps cannot be turned off. To avoid needing
# those in the manylinux container we just make our own cmake script as a
# workaround. If it weren't for that, the following would also work (from the
# root cmakelists.txt, instead of including this):
#
# add_subdirectory(libqasm/src/library)
# target_include_directories(lexgramstatic PUBLIC libqasm/src/library)

set(LIBQASM_SRC ${PROJECT_SOURCE_DIR}/libqasm/src/library)

find_package(BISON 3.0)
find_package(FLEX 2.6)

bison_target(
    libqasm_parser
    ${LIBQASM_SRC}/grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/grammar.tab.c
    COMPILE_FLAGS "-Wall -d -t -g -r all"
)
flex_target(
    libqasm_scanner
    ${LIBQASM_SRC}/lex.l
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
)
add_flex_bison_dependency(libqasm_scanner libqasm_parser)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/grammar.tab.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c PROPERTIES LANGUAGE CXX)
add_library(libqasm STATIC ${CMAKE_CURRENT_BINARY_DIR}/grammar.tab.c ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
target_include_directories(libqasm PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${LIBQASM_SRC})
set_property(TARGET libqasm PROPERTY CXX_STANDARD 11)
