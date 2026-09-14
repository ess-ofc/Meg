cmake_minimum_required(VERSION 3.20.0)
project(
   Meg VERSION 0.1.0
   LANGUAGES C
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   add_compile_definitions(DEBUG)
endif()

if (MEG_ENABLE_TESTING)
   enable_testing()
   message("Tests are enabled.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_POSITION_INDEPENDENT_CODE on)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED on)
set(CMAKE_C_EXTENSIONS off)

# Meg compiler
add_subdirectory(megc)
