
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include "cpumem_monitor.h" 
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread> 
using namespace std::chrono_literals;


 

int main()
{
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




	startwatching = false;
	t.join();
	return 0;
}
