cmake_minimum_required(VERSION 3.24.0)

set (CMAKE_CXX_STANDARD 20)

project(arma-gimp-plugin VERSION 0.1.0 LANGUAGES C CXX)

add_subdirectory(arma-file-formats-cxx)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GIMP REQUIRED gimp-3.0 gimpui-3.0 glib-2.0)

add_executable(arma-gimp-plugin main.cpp)

include_directories(arma-gimp-plugin ${GIMP_INCLUDE_DIRS})
target_link_libraries(arma-gimp-plugin ${GIMP_LIBRARIES})

target_link_libraries(arma-gimp-plugin aff_cxx_rust)
target_link_libraries(aff_cxx_rust PUBLIC ${GIMP_LIBRARIES})

if(WIN32)
    target_link_libraries(arma-gimp-plugin Bcrypt.lib)# Ntdll.lib Userenv.lib Ws2_32.lib Opengl32.lib Propsys.lib RuntimeObject.lib)
endif()

if(WIN32)
    if(NOT "$ENV{appdata}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{appdata}/GIMP/2.10/plug-ins/arma-gimp-plugin.exe" GIMP_PLUGIN_DIR)
    endif()
else()
    file(TO_CMAKE_PATH "$ENV{HOME}/.config/GIMP/3.0/plug-ins/arma-gimp-plugin/arma-gimp-plugin" GIMP_PLUGIN_DIR)
endif()

add_custom_command(TARGET arma-gimp-plugin POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:arma-gimp-plugin> ${GIMP_PLUGIN_DIR})