cmake_minimum_required(VERSION 3.15)
project(DepsDetector)

set(CMAKE_CXX_STANDARD 17)

set(
    SRC_FILES
    DependenciesSearcher.cpp
    inih/ini.c
    inih/cpp/INIReader.cpp
)

set(
    HEADERS_FILES
    inih/ini.h
    inih/cpp/INIReader.h
    TasksPool.h
)

include_directories(${CMAKE_SOURCE_DIR}/inih/cpp)

add_executable(
    ${PROJECT_NAME}
    ${SRC_FILES}
    ${HEADERS_FILES}
)

add_custom_command(
    TARGET ${PROJECT_NAME} 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/config.ini
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command(
    TARGET ${PROJECT_NAME} 
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/config.ini
        ${CMAKE_BINARY_DIR}/
)