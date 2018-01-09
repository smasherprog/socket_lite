cmake_minimum_required(VERSION 3.10)
project(socket_lite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(BUILD_SHARED_LIBS "Build shared library" OFF) 

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	add_definitions(-D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
	add_definitions(-D_SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING)
	
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
endif()

if(WIN32)
    set(SOCKET_LITE_PLATFORM_SRC     
		include/windows/IO_Context.h 
		src/windows/IO_Context.cpp
		include/windows/Socket.h
		src/windows/Socket.cpp
		include/windows/ListenContext.h
		src/windows/ListenContext.cpp
		include/windows/ClientContext.h
		src/windows/ClientContext.cpp  
    )
    set(SOCKET_LITE_PLATFORM_INC  
	include/windows 
    )

elseif(APPLE)
	
    set(SOCKET_LITE_PLATFORM_SRC 
    )
    set(SOCKET_LITE_PLATFORM_INC
		include/ios 
    )
else()
	set(SOCKET_LITE_PLATFORM_SRC 
    )
	set(SOCKET_LITE_PLATFORM_INC
		include/linux 
    )	
endif()

	
include_directories(
	include
	${SOCKET_LITE_PLATFORM_INC}
)
add_library(${PROJECT_NAME}  
	include/Socket_Lite.h
	include/common/SpinLock.h
	include/common/List.h
	src/Socket_Lite.cpp  
	${SOCKET_LITE_PLATFORM_SRC}
)

if(WIN32) 
	add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif() 

if(${BUILD_SHARED_LIBS})
	set_target_properties(${PROJECT_NAME} PROPERTIES DEFINE_SYMBOL SOCKET_LITE_DLL)
	if(WIN32) 
		install (FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION bin OPTIONAL)
		target_link_libraries(${PROJECT_NAME} Ws2_32 )
	elseif(APPLE) 
		find_package(Threads REQUIRED)
		target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
	else()
		find_package(Threads REQUIRED)
		target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} dl)
	endif()
endif()

install (TARGETS ${PROJECT_NAME} 
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib)

install (FILES 
	DESTINATION include)
	
enable_testing() 
add_subdirectory(test)

