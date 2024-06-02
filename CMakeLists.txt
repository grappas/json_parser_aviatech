cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME json_parser_aviatech)
project(${PROJECT_NAME} C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Add optimization flags
set(CMAKE_C_FLAGS_DEBUG "-g -Og")
set(CMAKE_C_FLAGS_RELEASE "-O3 -fdata-sections -ffunction-sections")

# Add the libgpiod static library
find_library(GPIOD_LIBRARY NAMES libgpiod.a gpiod PATHS /usr/lib /usr/local/lib)
message(STATUS "GPIOD_LIBRARY: ${GPIOD_LIBRARY}")
# Check if the library was found
if (NOT GPIOD_LIBRARY)
    message(FATAL_ERROR "libgpiod not found")
endif()

# Add the include path for libgpiod
include_directories(/usr/include /usr/local/include)

# Add the nmea_parser as a submodule
add_subdirectory(extern/nmea_parser)

# Add the include path for nmea_parser
include_directories(extern/nmea_parser)

# Add the executable
add_executable(${PROJECT_NAME} src/main.c extern/nmea_parser/nmea_parser.c)

target_compile_definitions(${PROJECT_NAME} PRIVATE NMEA_PRINT=0 NMEA_GSV_ENABLED=0 NMEA_VTG_ENABLED=0 NMEA_GLL_ENABLED=0 NMEA_GSA_ENABLED=0)

# Link libraries
# target_link_libraries(${PROJECT_NAME} nmea_parser)
target_link_libraries(${PROJECT_NAME} ${GPIOD_LIBRARY} nmea_parser)
