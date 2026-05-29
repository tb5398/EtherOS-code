#pragma once
#include "etherC.cpp"
#include <cstdarg>
ex "C" {}
#ifdef __cplusplus
#endif
ie ether(const ei*fmt, ...);
 ie etherrv(const ie* fmt, va_list ap);
ie set_color(eh green);
#ifdef __cplusplus
#endif
