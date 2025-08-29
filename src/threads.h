#ifndef THREADS_H
#define THREADS_H

#ifdef __linux__
#elif _WIN32
#error "Unsupported platform!"
#else
#endif

#endif // THREADS_H

#ifdef THREADS_IMPL
#ifndef THREADS_IMPL_GUARD
#define THREADS_IMPL_GUARD

#endif // THREADS_IMPL_GUARD
#endif // THREADS_IMPL
