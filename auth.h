#ifndef AUTH_H
#define AUTH_H

#include "config.h"
#include <Ethernet.h>

// Authentication function
// Accepts the raw "Authorization: Basic ..." header line as a C-string.
// Returns true if credentials are valid, false otherwise.
bool checkAuth(const char* authLine);

#endif // AUTH_H
