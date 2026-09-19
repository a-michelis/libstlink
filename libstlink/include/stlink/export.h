/**
 * @file    export.h
 * @brief   Symbol visibility.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Mark every type and function that belongs to the public API with STLINK_API.
 * Anything without it stays internal to the library.
 *
 * STLINK_EXPORT is defined only while the library itself is being compiled, so
 * the same header declares an export there and an import to everyone else.
 * Neither applies to a static build, which has no import to describe.
 */

#ifndef STLINK_EXPORT_H
#define STLINK_EXPORT_H

#if defined(STLINK_STATIC)
#define STLINK_API
#elif defined(_WIN32)
#if defined(STLINK_EXPORT)
#define STLINK_API __declspec(dllexport)
#else
#define STLINK_API __declspec(dllimport)
#endif
#else
#define STLINK_API __attribute__((visibility("default")))
#endif

#endif // STLINK_EXPORT_H
