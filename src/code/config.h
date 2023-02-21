#ifndef CPML_CONFIG
#define 

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if __has_feature(cxx_exceptions) || defined(__cpp_exceptions) || \
    (defined(_MSC_VER) && defined(_CPPUNWIND))
#define MPARK_PATTERNS_EXCEPTIONS
#endif

#endif CPML_CONFIG
