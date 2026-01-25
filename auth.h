#ifndef AUTH_H
#define AUTH_H

#include "config.h"
#include <Ethernet.h>

// Authentication function
bool checkAuth(String httpRequest);

#endif // AUTH_H