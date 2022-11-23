#ifndef RELIABLE_OVER_UDP_LOG_H
#define RELIABLE_OVER_UDP_LOG_H

#include <iostream>
#include <syncstream>
#include <iomanip>

#define LOG (std::osyncstream(std::cout) << "[" << std::setw(50) << __FUNCTION__ << "] ")

#endif //RELIABLE_OVER_UDP_LOG_H
