#include <stdbool.h>
#include <stdint.h>
#include <string.h>

unsigned long long __atomic_fetch_add_8(volatile void *ptr, unsigned long long val, int memorder)
{
    (void)memorder;
    volatile unsigned long long *p = (volatile unsigned long long *)ptr;
    unsigned long long old = *p;
    *p = old + val;
    return old;
}

void __atomic_load_8(const volatile void *ptr, void *ret, int memorder)
{
    (void)memorder;
    memcpy(ret, (const void *)ptr, sizeof(unsigned long long));
}

void __atomic_store_8(volatile void *ptr, void *val, int memorder)
{
    (void)memorder;
    memcpy((void *)ptr, val, sizeof(unsigned long long));
}

bool __atomic_compare_exchange_8(volatile void *ptr, void *expected, unsigned long long desired, bool weak,
                                 int success_memorder, int failure_memorder)
{
    (void)weak;
    (void)success_memorder;
    (void)failure_memorder;
    volatile unsigned long long *p = (volatile unsigned long long *)ptr;
    unsigned long long exp = *(unsigned long long *)expected;
    if (*p == exp) {
        *p = desired;
        return true;
    }
    *(unsigned long long *)expected = *p;
    return false;
}
