cmake_minimum_required(VERSION 3.10)
project(socket_lite_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
 include_directories(
	./
	../include
)
add_executable(${PROJECT_NAME} 
	../include/Socket_Lite.h   
	../include/include/defs.h 
	../include/include/Context.h 
	../include/include/spinlock.h 
	../include/include/PlatformSocket.h 
	../include/include/AsyncAcceptor.h 
	../include/include/AsyncPlatformSocket.h 
	../include/include/AwaitablePlatformSocket.h
	../include/include/AwaitableContext.h
	../include/include/Internal.h
	main.cpp 
	asio_echotest.h 
	asio_connectiontest.h  
	my_echotest.h 
	asio_echomodels.h 
	asio_transfertest.h 
	my_connectiontest.h 
	my_awaitconnectiontest.h
	my_awaitechotest.h
	my_awaittransfertest.h
	my_transfertest.h 
	my_multithreadedecho.h 
	my_echomodels.h 
	asio_multithreadedechotest.h 
	cpumem_monitor.h
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
