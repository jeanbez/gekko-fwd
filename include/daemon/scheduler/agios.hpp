#ifndef IFS_SCHEDULER_HPP
#define IFS_SCHEDULER_HPP

#include <agios.h>

extern struct client agios_client;

void agios_initialize();
void agios_shutdown();

void agios_callback(void *);
void agios_callback_aggregated(void **, int total);

unsigned long long int generate_unique_id();

#endif