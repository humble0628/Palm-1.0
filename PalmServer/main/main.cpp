#include "CServer.h"

int main() {
    try {
        boost::asio::io_context ioc;
        CServer server(ioc, 10086);
        ioc.run();
    }
    catch(std::exception& e) {
        std::cerr << "Exception is " << e.what() << std::endl;
    }

    return 0;
}