/**
 * @file    connect.h
 * @brief   Getting a debug connection to a target, and what state to leave it in.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. Entering debug is one command; connecting is a sequence, and the
 * three modes differ in what happens around that command rather than in the
 * command itself.
 *
 * This is not a programmer concern. Every step is an IProgrammer primitive,
 * and what makes a connection is the order they go in, so it composes above
 * the programmer rather than inside it.
 */

#ifndef STLINK_PV_CONNECT_H
#define STLINK_PV_CONNECT_H

#include <stlink/device.h>
#include <stlink/programmer.h>
#include <stlink/result.h>

namespace stlink
{
    /**
     * @brief Establish a debug connection, as @p options asks for.
     *
     * HotPlug attaches and does nothing else, so a running target keeps
     * running and is disturbed as little as attaching allows.
     *
     * Normal attaches and then resets, which is what most callers want: a
     * target in a known state. If the board does not wire nRST, that is
     * detected and a software reset is used instead.
     *
     * UnderReset holds the target in reset while attaching and catches the
     * core as it comes out, so it ends up halted at its reset vector having
     * executed nothing. This is the mode for a target whose firmware turns
     * the debug unit off, or sleeps, before an ordinary attach can get in.
     *
     * The access port is probed in every mode, because on some parts the CPU
     * is not on the default one and every later access would silently go to
     * the wrong place.
     *
     * @return ok when a debug connection exists, whatever the core is doing
     */
    [[nodiscard]] VoidResult connect_to_target(IProgrammer &programmer,
                                               const DeviceOptions &options);
} // namespace stlink

#endif // STLINK_PV_CONNECT_H
