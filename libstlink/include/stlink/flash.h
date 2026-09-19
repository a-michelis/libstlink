/**
 * @file    flash.h
 * @brief   The chip's flash controller.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Reached through IDevice::flash(), and owned by that device. How a given
 * family drives its controller, where its registers are and which of them it
 * has, stay inside the library; what is here is what every family can do.
 *
 * Option bytes and OTP live here rather than in a component of their own
 * because they are the same controller: writing them goes through the same
 * busy wait, the same error register and the same lock, differing only in
 * which key unlocks it.
 *
 * Nothing here is implicit. Erasing does not happen because a write needed
 * it, and nothing is verified unless verification was asked for. A caller
 * that wants the usual sequence asks for each step.
 */

#ifndef STLINK_FLASH_H
#define STLINK_FLASH_H

#include <cstddef>
#include <cstdint>
#include <functional>

#include <stlink/export.h>
#include <stlink/result.h>

namespace stlink
{
    /**
     * @brief How far a long running operation has got.
     *
     * Called from the thread that started the operation, never from another.
     * An empty one is fine and means the caller does not want telling.
     *
     * @param done  bytes finished
     * @param total bytes in the whole operation
     * @return false to abandon the operation, which then fails
     */
    using Progress = std::function<bool(std::size_t done, std::size_t total)>;

    /**
     * @brief The flash controller of one chip.
     *
     * One instance belongs to one thread at a time, and to the device that
     * owns it: it is valid for exactly as long as that device is.
     */
    class STLINK_API IFlash
    {
    public:
        virtual ~IFlash() = default;

        IFlash(const IFlash &) = delete;
        IFlash &operator=(const IFlash &) = delete;

        /* ---- Main flash ---------------------------------------------- */

        /**
         * @brief Erase every page that @p address and @p size touch.
         *
         * Erasing is by page, so a range that starts or ends inside one
         * erases the whole of it. ChipInfo::flash_page_size says how large
         * that is.
         */
        [[nodiscard]] virtual VoidResult erase(std::uint32_t address, std::size_t size,
                                                const Progress &progress = {}) = 0;

        /** @brief Erase the whole of main flash, which is faster than by page. */
        [[nodiscard]] virtual VoidResult erase_all(const Progress &progress = {}) = 0;

        /**
         * @brief Write to flash.
         *
         * The target range must already be erased. Alignment is the family's
         * business: a write that does not start or end on a boundary is
         * completed by reading back what surrounds it.
         */
        [[nodiscard]] virtual VoidResult write(std::uint32_t address,
                                                const std::uint8_t *data, std::size_t size,
                                                const Progress &progress = {}) = 0;

        /**
         * @brief Read back and compare, without writing anything.
         *
         * A failure says the contents differ, not that the operation could
         * not be carried out.
         */
        [[nodiscard]] virtual VoidResult verify(std::uint32_t address,
                                                 const std::uint8_t *data, std::size_t size,
                                                 const Progress &progress = {}) = 0;

        /* ---- Option bytes -------------------------------------------- */

        /**
         * @brief Whether this chip's option bytes can be reached.
         *
         * ChipInfo::option_bytes gives their address and size when they can.
         */
        [[nodiscard]] virtual bool has_option_bytes() const noexcept = 0;

        [[nodiscard]] virtual VoidResult read_option_bytes(std::uint8_t *data,
                                                            std::size_t size) = 0;

        /**
         * @brief Write the option bytes.
         *
         * These configure the chip itself: read protection, watchdog and boot
         * selection among them. Most chips apply them only after a power
         * cycle, and a wrong value can make the chip unreachable.
         */
        [[nodiscard]] virtual VoidResult write_option_bytes(const std::uint8_t *data,
                                                             std::size_t size) = 0;

        /* ---- One time programmable ----------------------------------- */

        /** @brief Whether this chip has an OTP area. ChipInfo::otp locates it. */
        [[nodiscard]] virtual bool has_otp() const noexcept = 0;

        [[nodiscard]] virtual VoidResult read_otp(std::uint32_t address,
                                                   std::uint8_t *data, std::size_t size) = 0;

        /**
         * @brief Write the one time programmable area.
         *
         * There is no erase, and no second attempt: what is written here
         * stays written for the life of the chip.
         */
        [[nodiscard]] virtual VoidResult write_otp(std::uint32_t address,
                                                    const std::uint8_t *data, std::size_t size) = 0;

    protected:
        IFlash() = default;
    };
} // namespace stlink

#endif // STLINK_FLASH_H
