#pragma once

#include <sys/types.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute a hash of the tuple of {source, remote} and {ip, port}.
 */
unsigned int JSNetHash(in_addr_t local_ip,
                       int local_port,
                       in_addr_t remote_ip,
                       int remote_port);

#ifdef __cplusplus
}
#endif
