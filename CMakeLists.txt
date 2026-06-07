cmake_minimum_required(VERSION 3.10)
project(CBLibrary C)

include(FetchContent)

FetchContent_Declare(OptionalAcornC GIT_REPOSITORY https://github.com/chrisbazley/OptionalAcornC.git GIT_TAG main)
FetchContent_Declare(Stream GIT_REPOSITORY https://github.com/chrisbazley/StreamLib.git GIT_TAG main)
FetchContent_Declare(CBOS GIT_REPOSITORY https://github.com/chrisbazley/CBOSLib.git GIT_TAG main)
FetchContent_Declare(CBUtil GIT_REPOSITORY https://github.com/chrisbazley/CBUtilLib.git GIT_TAG main)

FetchContent_MakeAvailable(OptionalAcornC Stream CBOS CBUtil)

set(CMAKE_C_STANDARD 99)
set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4 /wd4200 /wd4996 /wd4130)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -Wsign-conversion -pedantic)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy
        HINTS "/opt/homebrew/opt/llvm/bin"
    )

    if(CLANG_TIDY_EXE)
        set(CLANG_TIDY_ARGS "${CLANG_TIDY_EXE};-checks=-*,clang-analyzer-*")

        if(APPLE)
            execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE MAC_SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
            list(APPEND CLANG_TIDY_ARGS "--extra-arg=-isysroot" "--extra-arg=${MAC_SDK_PATH}")
        endif()

        set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_ARGS}")
        message(STATUS "Clang-Tidy Static Analyzer enabled automatically.")
    else()
        message(STATUS "clang-tidy binary not found. Skipping analysis pass.")
    endif()
endif()

set(OSUtilsList MsgTrans.c Canonical.c ScreenSize.c MakePath.c DateStamp.c ReadClock.c
    SetFType.c GetFType.c DirIter.c PathTail.c FileSize.c)

# Toolbox library utilities
set(ToolboxList StackViews.c ViewsMenu.c DeIconise.c GadgetHide.c GadgetFade.c
    DelOnEvent.c ReEventHan.c ReEventDel.c InputFocus.c)

# Desktop application file access with hourglass
set(DesktopIOList FilePerc.c AbortFOp.c FedCompMT.c LoadSaveMT.c FOpenCount.c)

# Generic desktop application functionality
set(DesktopList NoBudge.c Timer.c NullPoll.c Scheduler.c Err.c Drag.c
    Entity.c Loader2.c Saver.c Entity2.c Loader3.c Saver2.c Pal256.c UserData.c)

set(ObjectList ${OSUtilsList} ${ToolboxList} ${DesktopIOList} ${DesktopList})
set(XObjectList ErrNotRec.c FPCompOnly.c)
set(ObsoleteList ${ObjectList} FednetComp.c RoundRobin.c Loader.c)

file(GLOB ALL_HEADERS "*.h")
file(GLOB PRIVATE_HEADERS "Internal/*.h")

# Strip the deprecated components out for modern targets
set(PUBLIC_HEADERS ${ALL_HEADERS})
list(REMOVE_ITEM PUBLIC_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/Deprecated.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/FednetComp.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/RoundRobin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Loader.h"
)

add_library(CB STATIC ${ObjectList} ${PUBLIC_HEADERS} ${PRIVATE_HEADERS})

set_target_properties(CB PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    DEBUG_POSTFIX "dbg"
)

target_include_directories(CB PUBLIC .)

target_compile_definitions(CB PRIVATE
    $<$<CONFIG:Debug>:DEBUG_OUTPUT>
)

# Several header files depend on headers from dependencies
target_link_libraries(CB PUBLIC
    OptionalAcornC
    Stream
    CBOS
    CBUtil
)

install(TARGETS CB
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

add_library(CBObsolete STATIC ${ObsoleteList} ${ALL_HEADERS} ${PRIVATE_HEADERS})

set_target_properties(CBObsolete PROPERTIES
    PUBLIC_HEADER "${ALL_HEADERS}"
    DEBUG_POSTFIX "dbg"
)

target_include_directories(CBObsolete PUBLIC .)

target_compile_definitions(CBObsolete PRIVATE
    $<$<CONFIG:Debug>:DEBUG_OUTPUT>
)

target_compile_definitions(CBObsolete PRIVATE CBLIB_OBSOLETE)

# Several header files depend on headers from dependencies
target_link_libraries(CBObsolete PUBLIC
    OptionalAcornC
    Stream
    CBOS
    CBUtil
)

install(TARGETS CBObsolete
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

add_library(CBExtra STATIC ${XObjectList} ${PUBLIC_HEADERS} ${PRIVATE_HEADERS})

set_target_properties(CBExtra PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    DEBUG_POSTFIX "dbg"
)

target_include_directories(CBExtra PUBLIC .)

target_compile_definitions(CBExtra PRIVATE
    $<$<CONFIG:Debug>:DEBUG_OUTPUT>
)

# Several header files depend on headers from dependencies
target_link_libraries(CBExtra PUBLIC
    OptionalAcornC
    Stream
    CBOS
    CBUtil
)

install(TARGETS CBExtra
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

include(CTest)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_subdirectory(tests)
endif()
