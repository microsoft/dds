cmake_minimum_required(VERSION 3.10)

project(DDS_BackEnd VERSION 0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

## Set compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -march=native -Wall -Wextra -Werror -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion -Wno-old-style-cast -Wno-unused-function")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-deprecated-declarations")

## Add Redy front-end sources
include_directories(${CMAKE_SOURCE_DIR}/Include)
include_directories(${CMAKE_SOURCE_DIR}/Source)
set(DDS_BACKEND_SOURCES
    Source/base_cache_client.cc
    FrontEnd/cpu_affinity.cc
    FrontEnd/redy_cache_client.cc
    FrontEnd/request.cc)

## Add additional libraries
## ldl is for runtime shared library loading
set(LIBRARIES ${LIBRARIES} pthread dl)

if(DPDK_NEEDED STREQUAL "true")
    if (NOT WIN32)
        # Build the eRPC DPDK management daemon for Linux only
        add_executable(erpc_dpdk_daemon
	    eRPC/src/transport_impl/dpdk/dpdk_daemon.cc
	    eRPC/src/transport_impl/dpdk/dpdk_externs.cc
	    eRPC/src/transport_impl/dpdk/dpdk_init.cc)
        target_link_libraries(erpc_dpdk_daemon ${LIBRARIES})
    endif()
endif()

## Iterate the compilation options
function(string_starts_with str search)
    string(FIND "${str}" "${search}" out)
    if("${out}" EQUAL 0)
        return(true)
    endif()
    return(false)
endfunction()

if(EXISTS "${CMAKE_SOURCE_DIR}/config/config_build_options")
    FILE(READ "${CMAKE_SOURCE_DIR}/config/config_build_options" BUILD_OPTIONS)
    STRING(REGEX REPLACE ";" "\\\\;" BUILD_OPTIONS "${BUILD_OPTIONS}")
    STRING(REGEX REPLACE "\n" ";" BUILD_OPTIONS "${BUILD_OPTIONS}")
    foreach(O IN LISTS BUILD_OPTIONS)
        if(NOT O STREQUAL "")
            # Check if the path exists
            if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${O}")
                message(WARNING "Redy: ${O} not detected. Skipping...")
                continue()
            endif()

            # Check if this is an app
            string(REGEX MATCH "Apps/*" APP "${O}")
            string(LENGTH "${APP}" APP_LEN)
            if(${APP_LEN} EQUAL 0)
                # This is not an app. It is either Redy manager or Redy server
                if(O STREQUAL "BackEnd/CacheManager")
                    message(STATUS "Redy: Compiling Redy Manager...")
                    
                    # Include manager-specific headers
                    include_directories(${CMAKE_SOURCE_DIR}/BackEnd/CacheManager)
                    set(MANAGER_SOURCES
                        BackEnd/CacheManager/configuration_tree.cc
                        BackEnd/CacheManager/manager.cc)
                    
                    # Compile Redy manager
                    add_executable(RedyManager BackEnd/CacheManager/main.cc ${ERPC_SOURCES} ${MANAGER_SOURCES} ${UTILS_SOURCES})
                    target_link_libraries(RedyManager ${LIBRARIES})

                elseif(O STREQUAL "BackEnd/CacheServer")
                    message(STATUS "Redy: Compiling Redy Server...")

                    # Include server-specific headers
                    include_directories(${CMAKE_SOURCE_DIR}/BackEnd/CacheServer)
                    set(SERVER_SOURCES
                        BackEnd/CacheServer/server.cc)

                    # Include sproc-specific headers
                    include_directories(${CMAKE_SOURCE_DIR}/Sproc)
                    set(SPROC_SOURCES
                        Sproc/sproc.cc)
                    
                    # Compile Redy server
                    add_executable(RedyServer BackEnd/CacheServer/main.cc ${ERPC_SOURCES} ${SERVER_SOURCES} ${SPROC_SOURCES} ${UTILS_SOURCES})
                    target_link_libraries(RedyServer ${LIBRARIES})
                else()
                    message(WARNING "Redy: Unknown build option: ${O}. Skipping...")
                endif()
            else()
                string(LENGTH "${O}" O_LEN)
                math(EXPR APP_NAME_LEN "${O_LEN} - ${APP_LEN}")
                string(SUBSTRING "${O}" "${APP_LEN}" "${APP_NAME_LEN}" APP_NAME)
                message(STATUS "Redy: Compiling ${APP_NAME}...")

                # # Compile Redy front-end library
                # add_library(redy_frontend ${REDY_FRONTEND_SOURCES})

                # # Compile utils library
                # add_library(utils ${UTILS_SOURCES})

                # Include sproc-specific headers
                include_directories(${CMAKE_SOURCE_DIR}/Sproc)
                set(SPROC_SOURCES
                    Sproc/sproc.cc)
                
                # Compile the app
                add_executable(${APP_NAME} Apps/${APP_NAME}/main.cc ${ERPC_SOURCES} ${REDY_FRONTEND_SOURCES} ${SPROC_SOURCES} ${UTILS_SOURCES})
                target_link_libraries(${APP_NAME} ${LIBRARIES})
            endif()
        endif()
    endforeach()
else()
    message(STATUS "Redy: Nothing to build")
    message(STATUS "Redy: Specify the compilation with config/config_build_options")
    return()
endif()

## End of compilation

