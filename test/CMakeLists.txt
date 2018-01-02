project(socket_lite_test)

if(APPLE)
 
else()
 
	find_package(Threads REQUIRED)
	set(SOCKET_LITE_PLATFORM_LIBS
		${CMAKE_THREAD_LIBS_INIT}
	)
	
endif()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} socket_lite ${SOCKET_LITE_PLATFORM_LIBS})

add_test (NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
