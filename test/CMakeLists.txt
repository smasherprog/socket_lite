project(socket_lite_test)
include_directories(
	.
)
add_executable(${PROJECT_NAME} main.cpp asio_echotest.h asio_connectiontest.h  my_echotest.h asio_echomodels.h asio_transfertest.h my_connectiontest.h my_transfertest.h my_multithreadedecho.h my_echomodels.h asio_multithreadedechotest.h cpumem_monitor.h) 
find_package(Threads REQUIRED)
if(WIN32) 
	target_link_libraries(${PROJECT_NAME} Ws2_32 socket_lite)
elseif(APPLE) 
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} socket_lite)
else()
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} dl socket_lite)
endif()

add_test (NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
