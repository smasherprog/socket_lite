
#include "asio_echotest.h"
#include "asio_multithreadedechotest.h"
#include "asio_transfertest.h"
#include "my_connectiontest.h"
#include "my_echotest.h"
#include "my_multithreadedecho.h"
#include "my_transfertest.h"

int main(int argc, char *argv[])
{
    std::srand(std::time(nullptr));
    myconnectiontest::myconnectiontest();

    mymultithreadedechotest::myechotest();
    asio_multithreadedechotest::asioechotest();
    myechotest::myechotest();
    asiotest::asioechotest();
    asiotransfertest::asiotransfertest();
    mytransfertest::mytransfertest();

    return 0;
}
