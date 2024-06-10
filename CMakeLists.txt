

cmake_minimum_required(VERSION 3.15)


set(CMAKE_CXX_STANDARD 20)
set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL" CACHE PATH "OpenSSL Installation Folder")
set(ENV{OPENSSL_ROOT_DIR} ${OPENSSL_ROOT_DIR})


#set(ARCH_AVX on CACHE BOOL "Target CPU with AVX")
#set(ARCH_SSE on CACHE BOOL "Target CPU with SSE2")

#set(USE_WASMER on CACHE BOOL "Enable web assembly via wasmer.")








#find_package(OpenSSL REQUIRED)

project(xfer)

add_executable(xfer
	main.cpp
	secureSocketStream.h
	secureSocketStream.cpp
)

#set_target_properties(xfer PROPERTIES LINK_SEARCH_START_STATIC 1)
#set_target_properties(xfer PROPERTIES LINK_SEARCH_END_STATIC 1)
#set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
#find_library(SSL_LIB ssl_a REQUIRED)
#find_library(CRYPTO_LIB crypto_a REQUIRED)

#message(${SSL_LIB})
#message(${CRYPTO_LIB})

#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/openssl/build)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
	add_custom_command(
		OUTPUT
			${CMAKE_BINARY_DIR}/openssl/build/libssl.lib
			${CMAKE_BINARY_DIR}/openssl/build/libcrypto.lib
		#COMMAND ${CMAKE_SOURCE_DIR}/build_openssl_win32.bat
		COMMAND perl ${CMAKE_SOURCE_DIR}/openssl/Configure VC-WIN32 no-shared no-autoalginit no-pinshared -static
		COMMAND nmake
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openssl/build
	)
	add_custom_target(openssl DEPENDS ${CMAKE_BINARY_DIR}/openssl/build/libssl.lib ${CMAKE_BINARY_DIR}/openssl/build/libcrypto.lib)
	target_link_libraries(xfer ${CMAKE_BINARY_DIR}/openssl/build/libssl.lib ${CMAKE_BINARY_DIR}/openssl/build/libcrypto.lib)
else()
	add_custom_command(
		OUTPUT ${CMAKE_BINARY_DIR}/openssl/build/libssl.a ${CMAKE_BINARY_DIR}/openssl/build/libcrypto.a
		#COMMAND ${CMAKE_SOURCE_DIR}/build_openssl.sh
		COMMAND ${CMAKE_SOURCE_DIR}/openssl/Configure no-autoalginit no-shared no-pinshared -static
		COMMAND make -j$(nproc)
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/openssl/build
	)
	add_custom_target(openssl DEPENDS ${CMAKE_BINARY_DIR}/openssl/build/libssl.a ${CMAKE_BINARY_DIR}/openssl/build/libcrypto.a)
	target_link_libraries(xfer ${CMAKE_BINARY_DIR}/openssl/build/libssl.a ${CMAKE_BINARY_DIR}/openssl/build/libcrypto.a)
endif()

#add_dependencies(xfer openssl)
#target_dependencies(xfer openssl)

add_dependencies(xfer openssl)

target_include_directories(xfer PUBLIC openssl/include)
	#${SSL_LIB}
	#${CRYPTO_LIB}
	#)





#target_link_libraries(xfer ssl crypto)
#target_link_libraries(xfer OpenSSL::SSL OpenSSL::Crypto)
#target_link_libraries(xfer libssl_static.a libcrypto_static.a)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
	target_link_libraries(xfer ws2_32)
endif()


#add_library(dfp STATIC
#	include/dfp/all.h
#	include/dfp/pre.h
#	include/dfp/namespace.h
#	include/dfp/exceptions.h
#	include/dfp/macros.h
#
#	include/dfp/nstr.hpp
#
#	
#	include/dfp/wasm.hpp
#	src/wasm.cpp
#	include/dfp/WasmAllocator.h
#	src/WasmAllocator.cpp
#
#	
#
#	# From tsub
#	include/dfp/constexpr_utils.h
#	include/dfp/cow.hpp
#
#
#	include/dfp/hashing/hashing.hpp
#	include/dfp/hashing/sha256.hpp
#	include/dfp/hashing/sha1.hpp
#	include/dfp/hashing/murmur3.hpp
#	include/dfp/hashing/lcg.hpp
#	include/dfp/hashing/perfect.hpp
#
#
#	include/dfp/make_enum.h
#	include/dfp/stable_string_view.hpp
#	include/dfp/string_view_ref.hpp
#	include/dfp/u64.h
#	include/dfp/concepts.h
#	include/dfp/permuter.h
#
#	include/dfp/filebuffer.hpp
#
#	include/dfp/iflet.hpp
#
#
#	include/dfp/ranges/char_stream.hpp
#	include/dfp/ranges/NonOwningRange.hpp
#	include/dfp/ranges/equal.hpp
#
#
#	include/dfp/views/synth_macros.h
#	include/dfp/views/lexing.hpp
#	include/dfp/views/utf8.hpp
#
#
#	include/dfp/algo/RadixSort.h
#	include/dfp/algo/mergesort.hpp
#	include/dfp/algo/VerifySort.hpp
#	include/dfp/algo/levenshtein.hpp
#	include/dfp/algo/fft.h
#	src/algo/fft.cpp
#
#
#
#	# From Wigul
#	include/dfp/VirtualDestructor.h
#	include/dfp/containers/AutoArray.hpp
#
#	include/dfp/containers/RotBuffer.hpp
#	include/dfp/containers/InlineBuffer.hpp
#	include/dfp/containers/StringTable.hpp
#	include/dfp/containers/PerfectHashMap.hpp
#	include/dfp/containers/mkspan.hpp
#	include/dfp/containers/tinyvec.hpp
#
#
#	include/dfp/qalloc.h
#	src/qalloc/qalloc.c
#
#
#	include/dfp/cxstring.h
#	include/dfp/base64.h
#	src/base64.cpp
#
#	include/dfp/file.h
#	src/file.cpp
#
#	include/dfp/protobuf.h
#	src/protobuf.cpp
#
#
#	include/dfp/math/constants.hpp
#	include/dfp/math/feq.hpp
#	include/dfp/math/fixed.hpp
#	include/dfp/math/trig.hpp
#	include/dfp/math/vec.hpp
#	include/dfp/math/mat.hpp
#	include/dfp/math/quat.hpp
#
#	include/dfp/bit/graycode.hpp
#	include/dfp/bit/interleave.hpp
#	include/dfp/bit/BitFieldVector.hpp
#	include/dfp/bit/mask.hpp
#	include/dfp/bit/sim.hpp
#
#	include/dfp/thirdparty/cppcoro/generator.hpp
#
#)
#
##target_include_directories(dfp INTERFACE ${CMAKE_SOURCE_DIR}/include)
##target_include_directories(dfp PUBLIC ${CMAKE_SOURCE_DIR}/include)
#target_include_directories(dfp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
#
#
#
#add_executable(test_suite
#
#	include/dfp/all.h
#	src/test/test_framework.h
#
#	src/test/main.cpp
#	src/test/tempTest.cpp
#	src/test/test_RotBuffer.cpp
#	src/test/test_lexing.cpp
#	src/test/test_AutoArray.cpp
#	src/test/test_hashing.cpp
#	src/test/test_base64.cpp
#	src/test/test_math.cpp
#	src/test/test_levenshtein.cpp
#	src/test/test_RadixSort.cpp
#	src/test/test_MergeSort.cpp
#	src/test/test_BitFieldVector.cpp
#	src/test/test_LogicSim.cpp
#	src/test/test_iflet.cpp
#)
#
#target_compile_options(dfp PUBLIC -Wno-c++2b-extensions -Wno-unused-const-variable)
#string(TOLOWER ${CMAKE_CXX_COMPILER_ID} CMAKE_CXX_COMPILER_ID_LOWER)
#if (${CMAKE_CXX_COMPILER_ID_LOWER} MATCHES ".*clang.*")
#	target_compile_options(dfp PRIVATE "-fconstexpr-steps=9999999")
#endif()
#
#if(${USE_WASMER})
#	target_compile_definitions(dfp PUBLIC DFP_USE_WASMER=1)
#endif()
#
#if(${ARCH_AVX})
#	target_compile_definitions(dfp PUBLIC ARCH_AVX=1)
#	target_compile_definitions(dfp PUBLIC ARCH_SSE=1)
#	target_compile_options(dfp PUBLIC "-mavx")
#	target_compile_options(dfp PUBLIC "-msse")
#	target_compile_options(test_suite PUBLIC "-mavx")
#	target_compile_options(test_suite PUBLIC "-msse")
##endif()
#elseif(${ARCH_SSE})
#	target_compile_definitions(dfp PUBLIC ARCH_SSE=1)
#	target_compile_options(dfp, "-msse")
#	target_compile_options(test_suite, "-msse")
#endif()
#
#if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR ${CMAKE_BUILD_TYPE} STREQUAL "DEBUG")
#	target_compile_definitions(dfp PUBLIC DEBUG=1)
#	target_compile_definitions(test_suite PUBLIC DEBUG=1)
#endif()
#
## This is mainly a header library, let the consumer prebuild stuff
##target_precompile_headers(dfp PUBLIC include/dfp/pre.h)
#
#
#target_compile_definitions(test_suite PRIVATE CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
#target_link_libraries(test_suite dfp)
## target_compile_options(test_suite PRIVATE -fconcepts-diagnostics-depth=4)
#add_dependencies(test_suite dfp)
#
#add_test(NAME test_suite COMMAND test_suite "${CMAKE_CURRENT_SOURCE_DIR}")
#
##export(TARGETS dfp FILE "${PROJECT_BINARY_DIR}/dfp-config.cmake")
#export(TARGETS dfp FILE dfp-config.cmake)
##export(PACKAGE dfp)
