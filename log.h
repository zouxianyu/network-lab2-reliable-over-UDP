#ifndef RELIABLE_OVER_UDP_LOG_H
#define RELIABLE_OVER_UDP_LOG_H

#include <iostream>
#include <syncstream>
#include <iomanip>

#define LOG (std::osyncstream(std::cout) << \
            "[" << std::left << std::setw(50) <<__FUNCTION__ << std::right << "] ")

#endif //RELIABLE_OVER_UDP_LOG_H
