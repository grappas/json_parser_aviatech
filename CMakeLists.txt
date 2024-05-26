cmake_minimum_required(VERSION 3.10)
project(json_parser_aviatech C)

# Add the libgpiod static library
find_library(GPIOD_LIBRARY NAMES gpiod libgpiod.a PATHS /usr/lib /usr/local/lib)

# Add the include path for libgpiod
include_directories(/usr/include /usr/local/include)

# Add the nmea_parser as a submodule
add_subdirectory(extern/nmea_parser)

# Add the include path for nmea_parser
include_directories(extern/nmea_parser)

# Add the executable
add_executable(json_parser_aviatech src/main.c)

# Link libraries
target_link_libraries(json_parser_aviatech PRIVATE ${GPIOD_LIBRARY} nmea_parser)
