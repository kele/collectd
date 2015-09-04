#ifndef WMI_VARIANT_UTILS_H
#define WMI_VARIANT_UTILS_H

#include <oaidl.h>

double variant_get_double(VARIANT *v);
unsigned long long variant_get_ull(VARIANT *v);
uint64_t variant_get_uint64(VARIANT *v);
int64_t variant_get_int64(VARIANT *v);

#endif
