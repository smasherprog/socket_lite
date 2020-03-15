cmake_minimum_required(VERSION 3.14)
project(socket_lite_test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
  include_directories(
	./
	../include
	../
)
add_executable(${PROJECT_NAME}  
	../include/socket.h 
	../include/socket_io_events.h 
	../include/impl.h 
	../include/error_handling.h 
	../include/socket_address.h 

	../include/io_service.h 
	../include/io_thread_service.h  
	../Socket_Lite.h
	
	../test/my_echomodels.h
	../test/my_connectiontest.h 
	../test/my_multithreadedecho.h 
	main.cpp
) 
find_package(Threads REQUIRED)
if(WIN32) 
	add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /await")
	target_link_libraries(${PROJECT_NAME} Ws2_32)
elseif(APPLE) 
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -fcoroutines-ts")
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} dl)
endif()
enable_testing() 
add_test (NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
