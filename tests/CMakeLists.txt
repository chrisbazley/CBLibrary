file(GLOB SOURCES "*.c")
file(GLOB PRIVATE_HEADERS "*.h")

string(TOUPPER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME_UPPER)

if(SYSTEM_NAME_UPPER STREQUAL "RISCOS")
  # RISC OS: The only platform where it links and runs
  add_executable(CBTests ${SOURCES} ${PRIVATE_HEADERS})
  add_test(NAME "tests" COMMAND CBTests)
else()
  # Other platforms: Compile only (no linking) to catch errors
  add_library(CBTests OBJECT ${SOURCES} ${PRIVATE_HEADERS})
endif()

target_link_libraries(CBTests PRIVATE CB OptionalAcornC CBOS)
if(CBLIB_USE_CBDEBUG)
  target_compile_definitions(CBTests PRIVATE USE_CBDEBUG)
  target_link_libraries(CBTests PRIVATE CBDebug CBPseudo)
endif()

# Host tests exercise filenames and synthetic catalogue reads without RISC OS SWIs.
# Discard unused functions whose native implementations are unavailable here.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT USE_OPTIONAL AND
   CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(DirIterHostTests host/DirIterTest.c)
  target_link_libraries(DirIterHostTests PRIVATE strb CBUtil CBOS Stream OptionalAcornC)
  target_include_directories(DirIterHostTests PRIVATE ..)
  target_compile_options(DirIterHostTests PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(DirIterHostTests PRIVATE -Wl,--gc-sections)
  add_test(NAME DirIterHostTests COMMAND DirIterHostTests)
endif()

# Run the existing public-interface tests without the native-only test groups.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(PortableTests Main.c UserDTest.c PTailTest.c DecLExTest.c ../UserData.c ../PathTail.c)
  target_include_directories(PortableTests PRIVATE ..)
  target_link_libraries(PortableTests PRIVATE strb CBUtil CBOS Stream OptionalAcornC)
  target_compile_definitions(PortableTests PRIVATE CB_TEST_PORTABLE)
  add_test(NAME PortableTests COMMAND PortableTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT USE_OPTIONAL AND
   CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(CanonicalHostTests host/CanonicalTest.c ../CanonicSB.c)
  target_include_directories(CanonicalHostTests PRIVATE ..)
  target_link_libraries(CanonicalHostTests PRIVATE strb CBUtil CBOS OptionalAcornC)
  add_test(NAME CanonicalHostTests COMMAND CanonicalHostTests)
endif()
