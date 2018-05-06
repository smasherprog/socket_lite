
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include "asio_connectiontest.h"
#include "asio_echotest.h"
#include "asio_multithreadedechotest.h"
#include "asio_transfertest.h"
#include "cpumem_monitor.h"
#include "my_connectiontest.h"
#include "my_echotest.h"
#include "my_multithreadedecho.h"
#include "my_transfertest.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;

int main()
{
    std::cout << "Starting Network Benchmarks\n";
    std::srand(std::time(nullptr));

    myconnectiontest::myconnectiontest();

    return 0;
}
