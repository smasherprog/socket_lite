
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#if DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#include "cpumem_monitor.h" 
#include "my_awaitconnectiontest.h"
#include "my_multithreadedecho.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread> 
using namespace std::chrono_literals;

int main()
{
#if DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	std::cout << "Starting Network Benchmarks\n";
	std::cout << std::fixed;
	std::cout << std::setprecision(2);
	std::srand(std::time(nullptr));
	bool startwatching = true;
	float totalusage = 0.0f;
	float counts = 0.0f;
	SL::NET::CPUMemMonitor cpumemmom;
	cpumemmom.getCPUUsage();
	std::thread t([&] {
		while (startwatching) {
			auto temp = cpumemmom.getCPUUsage();
			totalusage += static_cast<float>(temp.ProcessUse);
			counts += 1.0f;
			std::this_thread::sleep_for(200ms);
		}
		});


	myawaitconnectiontest::myconnectiontest();
	std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
	counts = totalusage = 0;

	//mymultithreadedechotest::myechotest();
	//std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
	//counts = totalusage = 0;
	//mymultithreadedechotest::myechotest(1024 * 10); // 10k
	//std::cout << "Total: " << totalusage << " Avg:" << totalusage / counts << "%" << std::endl;
	//counts = totalusage = 0;

	startwatching = false;
	t.join();
	return 0;
}
