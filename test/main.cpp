
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include "CpuUsage.h"
#include "asio_connectiontest.h"
#include "asio_echotest.h"
#include "asio_multithreadedechotest.h"
#include "asio_transfertest.h"
#include "my_connectiontest.h"
#include "my_echotest.h"
#include "my_multithreadedecho.h"
#include "my_transfertest.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main()
{
    std::cout << "Starting Network Benchmarks\n";
    std::srand(std::time(nullptr));
    bool startwatching = true;
    std::thread t([&] {
        CpuUsage c;
        auto counter = 0;
        while (startwatching) {
            std::cout << "CPU " << c.GetUsage() << "%\n";
            std::this_thread::sleep_for(1s);
        }
    });

    myconnectiontest::myconnectiontest();
    asiotest::asioechotest();
    myechotest::myechotest();
    asiotransfertest::asiotransfertest();
    mytransfertest::mytransfertest();

    asio_multithreadedechotest::asioechotest();
    mymultithreadedechotest::myechotest();
    asioconnectiontest::connectiontest();
    startwatching = false;
    t.join();
    return 0;
}
