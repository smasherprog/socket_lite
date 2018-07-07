cmake_minimum_required(VERSION 3.10)
project(socket_lite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(
	include
)
add_library(${PROJECT_NAME}  
	include/Socket_Lite.h   
	include/Socket.h 
	include/defs.h 
	include/Context.h 
	include/spinlock.h 
	include/PlatformSocket.h 
	include/Listener.h 
	${SOCKET_LITE_PLATFORM_SRC}
)
 
install (TARGETS ${PROJECT_NAME} 
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib)

install (FILES 
	DESTINATION include)
	
enable_testing() 
add_subdirectory(test)

