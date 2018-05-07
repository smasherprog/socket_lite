
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
    bool startwatching = true;
    float totalusage = 0.0f;
    float counts = 0.0f;
    SL::NET::CPUMemMonitor cpumemmom;
    cpumemmom.getCPUUsage();
    std::thread t([&] {
        while (startwatching) {
            auto temp = cpumemmom.getCPUUsage();
            totalusage += temp.ProcessUse;
            counts += 1.0f;
            std::this_thread::sleep_for(200ms);
        }
    });

    myconnectiontest::myconnectiontest();
    std::cout << std::fixed;
    std::cout << std::setprecision(2);
    std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
    counts = totalusage = 0;

    // asiotest::asioechotest();
    // std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
    // counts = totalusage = 0;
    // myechotest::myechotest();
    // std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
    // counts = totalusage = 0;
    // asiotransfertest::asiotransfertest();
    // std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
    // counts = totalusage = 0;
    // mytransfertest::mytransfertest();
    // std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
    // counts = totalusage = 0;
    // asio_multithreadedechotest::asioechotest();
    // std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
    // counts = totalusage = 0;
    // mymultithreadedechotest::myechotest();
    // std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
    // counts = totalusage = 0;

    asioconnectiontest::connectiontest();
    std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
    counts = totalusage = 0;
    startwatching = false;
    t.join();
    return 0;
}
