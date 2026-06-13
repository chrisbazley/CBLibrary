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
