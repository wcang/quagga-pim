#include <arpa/inet.h>
#include <time.h>


/* Re-entry unsafe function to convert IPv6 address to string. String returned
 * from the function don't need to be freed.
 * Safe to be used for quagga since quagga is single threaded at this moment
 */
char *  in6_addr2str(struct in6_addr * addr);

static inline void time_inc(struct timeval * tv, unsigned int seconds)
{
  tv->tv_sec += seconds;
}
