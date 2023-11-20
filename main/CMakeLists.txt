file(GLOB SOURCES *.c *.cpp *.S)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "")
