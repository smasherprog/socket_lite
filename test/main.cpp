
#include "asio_echotest.h"
#include "asio_transfertest.h"
#include "my_echotest.h"
#include "my_transfertest.h"

int main(int argc, char *argv[])
{
    myechotest::myechotest();
    asiotest::asioechotest();
    mytransfertest::mytransfertest();
    asiotransfertest::asiotransfertest();
    int k = 0;
    std::cin >> k;
    return 0;
}
