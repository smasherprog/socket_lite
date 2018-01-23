project(socket_lite_test)


add_executable(${PROJECT_NAME} main.cpp asio_echotest.h my_echotest.h asio_transfertest.h my_connectiontest.h my_transfertest.h my_multithreadedecho.h asio_multithreadedechotest.h) 
find_package(Threads REQUIRED)
if(WIN32) 
	target_link_libraries(${PROJECT_NAME} Ws2_32 socket_lite)
elseif(APPLE) 
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} socket_lite)
else()
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} dl socket_lite)
endif()

add_test (NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
