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

static int is_valid_wii_ram(const void* p)
{
    uintptr_t a = (uintptr_t)p;

    if (a >= 0x80000000 && a <= 0x817FFFFF) return 1;
    if (a >= 0xC0000000 && a <= 0xC17FFFFF) return 1;
    if (a >= 0x90000000 && a <= 0x93FFFFFF) return 1;
    if (a >= 0xD0000000 && a <= 0xD3FFFFFF) return 1;

    return 0;
}

void __atomic_load_8(const volatile void *ptr, void *ret, int memorder)
{
    (void)memorder;

    // ret muss gültig sein
    if (!ret)
        return;

    if (!is_valid_wii_ram(ret))
        return;

    uint8_t* d = (uint8_t*)ret;

    // default = 0
    for (int i = 0; i < 8; i++)
    {
        if (!is_valid_wii_ram(d + i))
            return;

        d[i] = 0;
    }

    // ptr prüfen
    if (!ptr)
        return;

    if (!is_valid_wii_ram(ptr))
        return;

    const volatile uint8_t* s = (const volatile uint8_t*)ptr;

    for (int i = 0; i < 8; i++)
    {
        if (!is_valid_wii_ram(s + i))
            return;

        if (!is_valid_wii_ram(d + i))
            return;

        d[i] = s[i];
    }
}

void __atomic_store_8(volatile void *ptr, void *val, int memorder)
{
    (void)memorder;
    if (ptr != NULL && val != NULL) {
        unsigned long long temp = *(const unsigned long long *)val;
        memcpy((void *)ptr, &temp, sizeof(unsigned long long));
    }
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
