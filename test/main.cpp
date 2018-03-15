
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include "asio_connectiontest.h"
#include "asio_echotest.h"
#include "asio_multithreadedechotest.h"
#include "asio_transfertest.h"
#include "my_connectiontest.h"
#include "my_echotest.h"
#include "my_multithreadedecho.h"
#include "my_transfertest.h"

int main()
{
    std::srand(std::time(nullptr));

    myconnectiontest::myconnectiontest();

    asiotest::asioechotest();
    myechotest::myechotest();

    asiotransfertest::asiotransfertest();

    asio_multithreadedechotest::asioechotest();
    mymultithreadedechotest::myechotest();

    mytransfertest::mytransfertest();
    asioconnectiontest::connectiontest();
    return 0;
}
