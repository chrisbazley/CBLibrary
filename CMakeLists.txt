cmake_minimum_required(VERSION 3.10)
project(CBLibrary C)

include(FetchContent)

FetchContent_Declare(OptionalAcornC GIT_REPOSITORY https://github.com/chrisbazley/OptionalAcornC.git GIT_TAG main)
FetchContent_Declare(Stream GIT_REPOSITORY https://github.com/chrisbazley/StreamLib.git GIT_TAG main)
FetchContent_Declare(CBOS GIT_REPOSITORY https://github.com/chrisbazley/CBOSLib.git GIT_TAG main)
FetchContent_Declare(CBUtil GIT_REPOSITORY https://github.com/chrisbazley/CBUtilLib.git GIT_TAG main)

FetchContent_MakeAvailable(OptionalAcornC Stream CBOS CBUtil)

set(CMAKE_C_STANDARD 99)

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -Wsign-conversion -pedantic)
endif()

file(GLOB SOURCES "*.c")
file(GLOB PUBLIC_HEADERS "*.h")

add_library(CB STATIC ${SOURCES} ${PUBLIC_HEADERS})

set_target_properties(CB PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    DEBUG_POSTFIX "dbg"
)

target_include_directories(CB PUBLIC .)

target_compile_definitions(CB PRIVATE
    $<$<CONFIG:Debug>:DEBUG_OUTPUT>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(CB PRIVATE CBLIB_OBSOLETE)


target_compile_options(CB PRIVATE
    $<$<CONFIG:Release>:-O3>
)

target_link_libraries(CB PRIVATE
    OptionalAcornC
    Stream
    CBOS
    CBUtil
)

install(TARGETS CB
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
