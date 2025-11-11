#include "router.hpp"

#include <rtc/rtc.hpp>

int main() {
    rtc::InitLogger(rtc::LogLevel::Debug);
    sfu::Router router;
    router.Run();

    return 0;
}
