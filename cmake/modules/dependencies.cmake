if(DEPENDENCIES_FOUND)
    return()
endif(DEPENDENCIES_FOUND)

message("Search dependencies...")

# Extern libraries

if(NOT TARGET boostlog)
    find_package(BoostLog)
    if(BOOSTLOG_FOUND)
        add_library(boostlog SHARED IMPORTED)
        set_property(TARGET boostlog PROPERTY IMPORTED_LOCATION ${BOOSTLOG_LIBRARY})
        add_library(boostlogsetup SHARED IMPORTED)
        set_property(TARGET boostlogsetup PROPERTY IMPORTED_LOCATION ${BOOSTLOGSETUP_LIBRARY})
        add_library(boostthread SHARED IMPORTED)
        set_property(TARGET boostthread PROPERTY IMPORTED_LOCATION ${BOOSTTHREAD_LIBRARY})
        add_library(boostsystem SHARED IMPORTED)
        set_property(TARGET boostsystem PROPERTY IMPORTED_LOCATION ${BOOSTSYSTEM_LIBRARY})
        add_library(boostfilesystem SHARED IMPORTED)
        set_property(TARGET boostfilesystem PROPERTY IMPORTED_LOCATION ${BOOSTFILESYSTEM_LIBRARY})
        add_definitions(-DBOOST_LOG_DYN_LINK -DBOOST_SYSTEM_USE_UTF8)
    else(BOOSTLOG_FOUND)
        message(FATAL_ERROR "Cannot find extern library boostlog")
    endif(BOOSTLOG_FOUND)
endif(NOT TARGET boostlog)

if(NOT TARGET proj)
    find_package(Proj)
    if(PROJ_FOUND)
        add_library(proj SHARED IMPORTED)
        set_property(TARGET proj PROPERTY IMPORTED_LOCATION ${PROJ_LIBRARY})
    else(PROJ_FOUND)
        message(FATAL_ERROR "Cannot find extern library proj")
    endif(PROJ_FOUND)
endif(NOT TARGET proj)

if(NOT TARGET rok4)
    find_package(Rok4)
    if(ROK4_FOUND)
        add_library(rok4 SHARED IMPORTED)
        set_property(TARGET rok4 PROPERTY IMPORTED_LOCATION ${ROK4_LIBRARY})
    else(ROK4_FOUND)
        message(FATAL_ERROR "Cannot find extern library rok4")
    endif(ROK4_FOUND)
endif(NOT TARGET rok4)

if(NOT TARGET curl)
    find_package(Curl)
    if(CURL_FOUND)
        add_library(curl SHARED IMPORTED)
        set_property(TARGET curl PROPERTY IMPORTED_LOCATION ${CURL_LIBRARY})
    else(CURL_FOUND)
        message(FATAL_ERROR "Cannot find extern library libcurl")
    endif(CURL_FOUND)
endif(NOT TARGET curl)

if(DOC_ENABLED)
  
  # Extern libraries, shared

    if(NOT TARGET Doxygen)
        find_package(Doxygen REQUIRED dot)
        if(DOXYGEN_FOUND)
            message(STATUS "Doxygen ${DOXYGEN_VERSION} found")
        else(DOXYGEN_FOUND)
            message(FATAL_ERROR "Cannot find extern tool doxygen")
        endif(DOXYGEN_FOUND)
    endif(NOT TARGET Doxygen)

endif(DOC_ENABLED)

set(DEPENDENCIES_FOUND TRUE BOOL)
