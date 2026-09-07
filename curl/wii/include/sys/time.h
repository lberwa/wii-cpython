#ifndef _WII_SYS_TIME_H
#define _WII_SYS_TIME_H
/* sys/types.h muss vor sys/time.h kommen (u_int-Definition).
   Wir definieren u_int auch direkt, falls sys/time.h den Wrapper ueberspringt. */
#include <sys/types.h>
#ifndef __u_int_defined
typedef unsigned int u_int;
#define __u_int_defined
#endif
#include_next <sys/time.h>
#endif
