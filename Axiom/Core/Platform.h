#pragma once

#if defined(_WIN32)
#define AXIOM_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
#define AXIOM_PLATFORM_MACOS 1
#elif defined(__linux__)
#define AXIOM_PLATFORM_LINUX 1
#else
#error "Axiom does not support this platform yet."
#endif

#ifndef AXIOM_PLATFORM_WINDOWS
#define AXIOM_PLATFORM_WINDOWS 0
#endif

#ifndef AXIOM_PLATFORM_MACOS
#define AXIOM_PLATFORM_MACOS 0
#endif

#ifndef AXIOM_PLATFORM_LINUX
#define AXIOM_PLATFORM_LINUX 0
#endif

#if (AXIOM_PLATFORM_WINDOWS + AXIOM_PLATFORM_MACOS + AXIOM_PLATFORM_LINUX) != 1
#error "Axiom platform detection must resolve to exactly one target platform."
#endif
