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
option(CMAKE_INSTALL_PREFIX "Installation location" "/usr/local")
option(OBJECT_ENABLED "Build with object storage" 0)
option(KDU_ENABLED "Build with kakadu" 0)
option(KDU_THREADING "Threading level for kakadu" 0)
option(DEBUG_BUILD "Build with debug mode" 0)
option(DEB_PACKAGE "Debian package compilation" 0)

################### VERSION
set(VERSION ${BUILD_VERSION})

message("Version ${VERSION}")

################### PACKAGE CONFIGURATION

set(CPACK_PACKAGE_NAME "ROK4GENERATION")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_PACKAGE_DESCRIPTION "WMS/WMTS/TMS server")
set(CPACK_PACKAGE_VENDOR "IGN")
set(CPACK_GENERATOR "TGZ")

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

message(STATUS "Building ROK4 Core")
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

# Dossiers contenant les headers nécessaires
include_directories(lib/core/src)
include_directories(${PROJECT_BINARY_DIR} ${BOOSTLOG_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${PROJ_INCLUDE_DIR})

foreach(cmd IN LISTS ROK4GENERATION_COMMANDS)
    message(STATUS "Building ROK4 GENERATION command ${cmd}")
    add_executable(${cmd} ${PROJECT_SOURCE_DIR}/src/${cmd}.cpp)
    target_link_libraries(${cmd} PUBLIC rok4core boostlogsetup boostthread boostfilesystem boostsystem)
endforeach()

################### INSTALLATION

INSTALL(TARGETS ${ROK4GENERATION_COMMANDS}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

################### PACKAGES

if(DEB_PACKAGE)

    message(STATUS "Building a debian package")

    set(CPACK_DEBIAN_PACKAGE_NAME "rok4-generation")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "IGN, Géoportail (<tout_rdev@ign.fr>)")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "ROK4 pyramids processing utils")
    set(CPACK_DEBIAN_PACKAGE_SECTION "science")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

    set(CPACK_DEBIAN_PACKAGE_DEPENDS "zlib1g-dev ,libcurl4-openssl-dev ,libproj-dev ,libssl-dev ,libturbojpeg0-dev ,libjpeg-dev ,libc6-dev ,librados-dev ,libjson11-1-dev ,libboost-log-dev ,libboost-filesystem-dev ,libboost-system-dev, libsqlite3-dev, libopenjp2-7-dev, libpng-dev, libtiff5-dev")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    else()
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    endif()

    set( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${ROK4GENERATION_SOURCE_DIR}/cmake/debian/postinst;" )

    set(CPACK_BINARY_DEB "ON")
    set(CPACK_GENERATOR "DEB")

endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_ARCHITECTURE "64bit")
else()
    set(BUILD_ARCHITECTURE "32bit")
endif()

set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${BUILD_ARCHITECTURE}")

include(CPack)