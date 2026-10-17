cmake_minimum_required(VERSION 3.13)
project( gameNetwork )

include_directories( 
	${LuaIncludePath}
	${CORONA_ROOT}/librtt/Corona
)

lua_2_c("${CORONA_ROOT}/plugins/gameNetwork/shared/gameNetwork.lua")
lua_2_c("${CORONA_ROOT}/plugins/gameNetwork/shared/CoronaProvider.gameNetwork.lua")

add_library( gameNetwork SHARED 
				../shared/CoronaGameNetworkLibrary.cpp
				${Lua2CppOutputDir}/gameNetwork.c
				${Lua2CppOutputDir}/CoronaProvider.gameNetwork.c )

if(ANDROID AND (ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "x86_64"))
	# NDK version < 28
	target_link_options(gameNetwork PRIVATE "-Wl,-z,max-page-size=16384")
	# NDK version <= 22
	target_link_options(gameNetwork PRIVATE "-Wl,-z,common-page-size=16384")
endif()
target_link_libraries ( gameNetwork lua corona )
