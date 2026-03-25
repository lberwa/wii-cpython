/* Wii-specific mbedTLS overrides for cross-compilation with libogc. */
#ifndef MBEDTLS_WII_USER_CONFIG_H
#define MBEDTLS_WII_USER_CONFIG_H

/* libogc is neither Unix nor Windows: use external millisecond timer hook. */
#ifndef MBEDTLS_PLATFORM_MS_TIME_ALT
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#endif

/*
 * Builtin entropy backends depend on Unix/Windows platform APIs.
 * For Wii we require an external entropy provider at final link time.
 */
#ifdef MBEDTLS_PSA_BUILTIN_GET_ENTROPY
#undef MBEDTLS_PSA_BUILTIN_GET_ENTROPY
#endif
#ifndef MBEDTLS_PSA_DRIVER_GET_ENTROPY
#define MBEDTLS_PSA_DRIVER_GET_ENTROPY
#endif

/* Wii sockets are provided by libogc, not mbedTLS net_sockets.c. */
#ifdef MBEDTLS_NET_C
#undef MBEDTLS_NET_C
#endif

#endif /* MBEDTLS_WII_USER_CONFIG_H */
