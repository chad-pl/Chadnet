set(CHAD_PRELUDE_NAMES fn print show math hear)

set(CHAD_PRELUDE_FILES "")
foreach(name ${CHAD_PRELUDE_NAMES})
    set(path ${CMAKE_CURRENT_SOURCE_DIR}/lang/prelude/${name}.chad)
    file(READ ${path} source)
    string(APPEND CHAD_PRELUDE_FILES "    {\"prelude/${name}.chad\", R\"chad_prelude(${source})chad_prelude\"},\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${path})
endforeach()

configure_file(lang/prelude.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/prelude.h @ONLY)

add_library(chad_core
    core/error.cpp
    core/file.cpp
    core/text.cpp
    lang/lexer.cpp
    lang/parser.cpp
    lang/syntax.cpp
    lang/compile.cpp
    net/engine.cpp
    net/expr.cpp
    net/net.cpp
    runtime/io.cpp
    runtime/utf8.cpp
    runtime/input.cpp
)
add_library(chad::core ALIAS chad_core)

target_include_directories(chad_core
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(chad_core PRIVATE chad_options)