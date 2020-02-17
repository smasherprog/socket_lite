cmake_minimum_required(VERSION 3.14)
project(socket_lite_test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
  include_directories(
	./
	../include
)
add_executable(${PROJECT_NAME}  
	../include/ip_address.h
	../include/ip_endpoint.h
	../include/ipv4_address.h
	../include/ipv4_endpoint.h
	../include/ipv6_address.h
	../include/ipv6_endpoint.h
	../include/socket.h 
	../include/io_service.h
	../include/utils.h 

	../include/socket_accept_operation.h
	../include/socket_connect_operation.h
	../include/socket_disconnect_operation.h
	../include/socket_recv_from_operation.h
	../include/socket_recv_operation.h
	../include/socket_send_operation.h
	../include/socket_send_to_operation.h

	../Socket_Lite.h
	../src/ipv4_address.cpp
	../src/ipv4_endpoint.cpp
	../src/ipv6_address.cpp
	../src/ipv6_endpoint.cpp
	../src/socket.cpp
	../src/io_service.cpp

	../src/socket_accept_operation.cpp
	../src/socket_connect_operation.cpp
	main.cpp
) 
find_package(Threads REQUIRED)
if(WIN32) 
	add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /await")
	add_definitions(-D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
	add_definitions(-D_SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING)
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
