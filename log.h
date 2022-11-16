#ifndef RELIABLE_OVER_UDP_LOG_H
#define RELIABLE_OVER_UDP_LOG_H

#include <iostream>
#include <iomanip>

#define LOG (std::cout << "[" << std::setw(15) << __FILE__ << ":" << __LINE__ << "] ")

#endif //RELIABLE_OVER_UDP_LOG_H
