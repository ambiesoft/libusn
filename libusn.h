#pragma once

#ifdef LIBUSN_EXPORTS
#define DECLAIR_LIBUSN __declspec(dllexport)
#else
#define DECLAIR_LIBUSN __declspec(dllimport)
#endif

DECLAIR_LIBUSN bool hasAnyChange(const wchar_t* pDirectory);