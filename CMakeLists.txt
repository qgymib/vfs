cmake_minimum_required(VERSION 3.5)
project(vfs)

option(VFS_ASAN "Enable AddressSanitizer" OFF)

###############################################################################
# Functions
###############################################################################
function(vfs_setup_target_wall name)
    if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name} PRIVATE /W4 /WX)
    else ()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    endif ()
endfunction()

function(vfs_setup_asan name)
    if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name} PUBLIC /fsanitize=address)
        target_link_options(${name} PUBLIC /fsanitize=address)
    else ()
        target_compile_options(${name} PUBLIC -fsanitize=address)
        target_link_options(${name} PUBLIC -fsanitize=address)
    endif ()
endfunction()

###############################################################################
# Setup library
###############################################################################
add_library(${PROJECT_NAME}
    src/utils/atomic.c
    src/utils/list.c
    src/utils/map.c
    src/utils/mutex.c
    src/utils/rwlock.c
    src/utils/sem.c
    src/utils/thread.c
    src/utils/threadpool.c
    src/vfs.c
    src/vfs_inner.c
    src/vfs_visitor.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

vfs_setup_target_wall(${PROJECT_NAME})

if (VFS_ASAN)
    vfs_setup_asan(${PROJECT_NAME})
endif ()

###############################################################################
# Test
###############################################################################
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(third_party/cutest)
	add_subdirectory(test)
endif()
