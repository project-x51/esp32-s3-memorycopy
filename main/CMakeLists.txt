set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB SOURCES *.c *.cpp *.S)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS ""
                       REQUIRES esp-dsp)
