cmake_minimum_required(VERSION 3.16)
project(cmdtab C)

set(C_STANDARD 99)
set(C_STANDARD_REQUIRED TRUE)
set(BUILD_TYPE "MinSizeRel")

add_executable(cmdtab WIN32 "src/cmdtab.c" "res/cmdtab.rc")

# cmdtab.rc already embeds res/cmdtab.manifest (resource type 24, id 1);
# keep the linker from embedding a second, conflicting default manifest
if(MSVC)
	target_link_options(cmdtab PRIVATE /MANIFEST:NO)
endif()

#set_property(TARGET ${PROJECT_NAME} PROPERTY
    #LINK_FLAGS /MANIFESTUAC:"level='highestAvailable' uiAccess='true'"
#)
#mt.exe -manifest "cmdtab.exe.manifest" -updateresource:"cmdtab.exe;"#1
#mt.exe -manifest "cmdtab.exe.manifest" -outputresource:"cmdtab.exe;"#1
