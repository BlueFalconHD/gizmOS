#pragma once

#include <lib/spinlock.h>

void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);


