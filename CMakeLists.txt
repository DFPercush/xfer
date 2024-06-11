cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 20)
project(xfer)

add_executable(xfer
	main.cpp
	secureSocketStream.h
	secureSocketStream.cpp
)


file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/openssl)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
	if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
		add_custom_command(
			OUTPUT ${CMAKE_BINARY_DIR}/openssl/configdata.pm
			COMMAND perl ${CMAKE_SOURCE_DIR}/openssl/Configure VC-WIN64A no-shared --release /PDB:NONE
			DEPENDS ${CMAKE_SOURCE_DIR}/openssl/Configure ${CMAKE_SOURCE_DIR}/CMakeLists.txt
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openssl
		)
	else()
		add_custom_command(
			OUTPUT ${CMAKE_BINARY_DIR}/openssl/configdata.pm
			COMMAND perl ${CMAKE_SOURCE_DIR}/openssl/Configure VC-WIN64A no-shared -static --release
			DEPENDS ${CMAKE_SOURCE_DIR}/openssl/Configure ${CMAKE_SOURCE_DIR}/CMakeLists.txt
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openssl
		)
	endif()
	add_custom_command(
		OUTPUT
			${CMAKE_BINARY_DIR}/openssl/libssl.lib
			${CMAKE_BINARY_DIR}/openssl/libcrypto.lib
		COMMAND nmake
		DEPENDS ${CMAKE_BINARY_DIR}/openssl/configdata.pm
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openssl
	)
	add_custom_target(openssl DEPENDS ${CMAKE_BINARY_DIR}/openssl/libssl.lib ${CMAKE_BINARY_DIR}/openssl/libcrypto.lib)
	target_link_libraries(xfer ${CMAKE_BINARY_DIR}/openssl/libssl.lib ${CMAKE_BINARY_DIR}/openssl/libcrypto.lib crypt32.lib)

#elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
else()
	add_custom_command(
		OUTPUT ${CMAKE_BINARY_DIR}/openssl/configdata.pm
		COMMAND ${CMAKE_SOURCE_DIR}/openssl/Configure  no-shared -static --release  # no-autoalginit   no-pinshared
		DEPENDS ${CMAKE_SOURCE_DIR}/openssl/Configure ${CMAKE_SOURCE_DIR}/CMakeLists.txt
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openssl
	)
	add_custom_command(
		OUTPUT
			${CMAKE_BINARY_DIR}/openssl/libssl.a
			${CMAKE_BINARY_DIR}/openssl/libcrypto.a
		COMMAND make -j$(nproc)
		DEPENDS ${CMAKE_BINARY_DIR}/openssl/configdata.pm
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openssl
	)
	add_custom_target(openssl DEPENDS ${CMAKE_BINARY_DIR}/openssl/libssl.a ${CMAKE_BINARY_DIR}/openssl/libcrypto.a)
	target_link_libraries(xfer ${CMAKE_BINARY_DIR}/openssl/libssl.a ${CMAKE_BINARY_DIR}/openssl/libcrypto.a)
#elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
#elseif(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
#elseif(CMAKE_SYSTEM_NAME STREQUAL NetBSD)
#elseif(CMAKE_SYSTEM_NAME STREQUAL OpenBSD)
#elseif(CMAKE_SYSTEM_NAME STREQUAL DragonFly)
#elseif(CMAKE_SYSTEM_NAME STREQUAL SunOS)
#elseif(CMAKE_SYSTEM_NAME STREQUAL AIX)
#elseif(CMAKE_SYSTEM_NAME STREQUAL HP-UX)
#elseif(CMAKE_SYSTEM_NAME STREQUAL IRIX)
#elseif(CMAKE_SYSTEM_NAME STREQUAL OSF)
#elseif(CMAKE_SYSTEM_NAME STREQUAL SCO)
#else()
#	panic("Unsupported platform")
endif()


add_dependencies(xfer openssl)

target_include_directories(xfer PUBLIC
	${CMAKE_BINARY_DIR}/openssl/include
	${CMAKE_SOURCE_DIR}/openssl/include
)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
	target_link_libraries(xfer ws2_32)
endif()

# TODO: For 32-bit systems
# This will only affect the build of xfer. Openssl must be configured through its perl script.
#set_target_properties(xfer PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")


