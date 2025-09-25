cmake_minimum_required(VERSION 3.16)
project(cmdtab C)

set(C_STANDARD 99)
set(C_STANDARD_REQUIRED TRUE)
set(BUILD_TYPE "Debug")

add_executable(cmdtab WIN32 "src/cmdtab.c" "res/cmdtab.rc")

# Copy cmdtab.ini to build directory
add_custom_command(TARGET cmdtab POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/cmdtab.ini"
        "$<TARGET_FILE_DIR:cmdtab>/cmdtab.ini"
        COMMENT "Copying cmdtab.ini to build directory"
)

#set_property(TARGET ${PROJECT_NAME} PROPERTY
    #LINK_FLAGS /MANIFESTUAC:"level='highestAvailable' uiAccess='true'"
#)
#mt.exe -manifest "cmdtab.exe.manifest" -updateresource:"cmdtab.exe;"#1
#mt.exe -manifest "cmdtab.exe.manifest" -outputresource:"cmdtab.exe;"#1
