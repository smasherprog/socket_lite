#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace mymultithreadedechotest {

void myechotest()
{
    std::cout << "Starting My 4 thread Echos Test" << std::endl;
    myechomodels::writeechos = 0;
    myechomodels::keepgoing = true;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    SL::NET::Context iocontext(SL::NET::ThreadCount(4));
    SL::NET::StatusCode ec;
    SL::NET::Acceptor acceptor(SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4, ec);
    if (ec != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Acceptor failed to create code:" << ec << std::endl;
    }
    std::vector<std::shared_ptr<myechomodels::session>> clients;
    acceptor.set_handler([&](SL::NET::Socket socket) {
        auto s = std::make_shared<myechomodels::session>(socket);
        s->do_read();
        s->do_write();
        clients.push_back(s);
    });
    SL::NET::Listener Listener(iocontext, std::move(acceptor));
     
    auto[code, addresses] = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    if (code != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Error code:" << code << std::endl;
    }
    myechomodels::asioclient c(iocontext, addresses);
    myechomodels::asioclient c1(iocontext, addresses);
    myechomodels::asioclient c2(iocontext, addresses);
    c.do_connect();
    c1.do_connect();
    c2.do_connect();

    iocontext.run();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    myechomodels::keepgoing = false;
    std::cout << "My 4 thread Echos per Second " << myechomodels::writeechos / 10 << std::endl;
    c.close();
    c1.close();
    c2.close(); 
    for (auto &p : clients) {
        p->close();
    }
}

} // namespace mymultithreadedechotest
