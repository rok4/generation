################### INITIALISATION

# check the cmake version
cmake_minimum_required(VERSION 3.10)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(BUILD_SHARED_LIBS OFF)

# Définit le nom du projet 
project(ROK4GENERATION)

################### PARAMÈTRES

option(BUILD_VERSION "Build version" 0.0.0)
option(OBJECT_ENABLED "Build with object storage" 0)
option(KDU_ENABLED "Build with kakadu" 0)
option(KDU_THREADING "Build with kakadu" 0)
option(DEBUG_BUILD "Build with debug mode" 0)

################### VERSION
set(ROK4_VERSION ${BUILD_VERSION})

message("Version ${ROK4_VERSION}")

################### PACKAGE CONFIGURATION

set(CPACK_PACKAGE_NAME "ROK4GENERATION")
set(CPACK_PACKAGE_VERSION ${ROK4_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "WMS/WMTS/TMS server")
set(CPACK_PACKAGE_VENDOR "IGN")

################### MODE DEBUG

if(DEBUG_BUILD)
  set(CMAKE_BUILD_TYPE debugbuild)
  set(CMAKE_CXX_FLAGS_DEBUGBUILD "-g -O0")
  set(CMAKE_C_FLAGS_DEBUGBUILD "-g")
else(DEBUG_BUILD)
  set(CMAKE_BUILD_TYPE specificbuild)
  set(CMAKE_CXX_FLAGS_SPECIFICBUILD "-O3")
endif(DEBUG_BUILD)

################### DÉPENDANCES

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})
message(STATUS "Modules CMake : ${CMAKE_MODULE_PATH}")

include(cmake/modules/dependencies.cmake)

################### COMPILATION

# Configuration du fichier (injection de la version et de la compilation objet)
configure_file(config.h.in config.h ESCAPE_QUOTES @ONLY)

message(STATUS "Building rok4core")
set(FILEIMAGE_ENABLED 1)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/core)

set(ROK4GENERATION_COMMANDS 
    cache2work
    checkWork
    composeNtiff
    decimateNtiff
    manageNodata
    merge4tiff
    mergeNtiff
    overlayNtiff
    pbf2cache
    work2cache
)

foreach(cmd IN LISTS ROK4GENERATION_COMMANDS)
    message(STATUS "Building ROK4 GENERATION command ${cmd}")
    add_executable(${cmd} ${PROJECT_SOURCE_DIR}/src/${cmd}.cpp)
    target_include_directories(${cmd} PUBLIC "${PROJECT_BINARY_DIR}")
    target_link_libraries(${cmd} PUBLIC rok4core boostlogsetup boostthread boostfilesystem boostsystem)
endforeach()

include_directories(lib/core/src)

################### TESTS

################### DOCUMENTATION

################### INSTALLATION

INSTALL(TARGETS ${ROK4GENERATION_COMMANDS} rok4core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_ARCHITECTURE "64bit")
else()
    set(BUILD_ARCHITECTURE "32bit")
endif()
set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${BUILD_ARCHITECTURE}")
include(CPack)