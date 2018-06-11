#ifndef SELECT_H
#define SELECT_H

#include "sys/types.h"
#include "sys/time.h"
#include "sys/unistd.h"

int select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout);


#endif
