/**
 * @file    connect.cpp
 * @brief   Getting a debug connection to a target.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The sequences here follow the C project's stlink_target_connect, because
 * they were arrived at against a great deal of hardware and the reasons for
 * each step are not always obvious from the manuals. Where this departs from
 * it, the departure is commented.
 */

#include <stlink/connect.h>

#include <chrono>
#include <thread>

#include <stlink/chip_identity.h>
#include <stlink/cortex.h>
#include <stlink/log.h>

namespace stlink
{
    namespace
    {
        /** @brief The minimum reset pulse, from RM0008 section 8.1.2. */
        constexpr auto kResetPulse = std::chrono::microseconds(20);

        /**
         * @brief How long to keep trying to halt a core coming out of reset.
         *
         * There is no event to wait on, so this is a race against the target's
         * own startup, and the only way to win it is to keep asking.
         */
        constexpr auto kCatchWindow = std::chrono::milliseconds(10);
        constexpr auto kCatchInterval = std::chrono::microseconds(100);

        /** @brief How long to let a reset settle before looking at the result. */
        constexpr auto kResetSettle = std::chrono::milliseconds(10);

        /**
         * @brief Point the programmer at whichever access port has the CPU.
         *
         * Nearly always the default one. Some parts, an STM32H5 among them,
         * expose the CPU only on AP1, and on those every debug access made on
         * AP0 fails quietly: the core never halts, and the checks that follow
         * report nonsense rather than failing. So this runs in every mode.
         *
         * Not being able to select an access port at all is not an error. A
         * V1 cannot, and a V1 is never one of the parts that needs it.
         */
        void probe_access_port(IProgrammer &programmer)
        {
            if (read_cpu_id(programmer).ok())
            {
                return;
            }

            if (!programmer.select_access_port(1).ok())
            {
                return;
            }

            if (read_cpu_id(programmer).ok())
            {
                STLINK_LOG_INF("the CPU is on access port 1");

                return;
            }

            /* Nothing there either; put it back so the error path is honest. */
            (void)programmer.select_access_port(0);
        }

        /** @brief Whether the core has been reset since DHCSR was last read. */
        bool saw_reset(IProgrammer &programmer)
        {
            auto dhcsr = programmer.read_debug_register(kDhcsr);

            return dhcsr.ok() && (dhcsr.value() & kDhcsrResetSince) != 0;
        }

        /**
         * @brief Reset the core through AIRCR rather than the reset pin.
         *
         * For boards that do not wire nRST, which is a great many of them.
         */
        VoidResult software_reset(IProgrammer &programmer, bool halt_on_reset)
        {
            if (halt_on_reset)
            {
                /*
                 * DEMCR is in the debug power domain, so this survives the
                 * reset it is arming, which is what makes catching the reset
                 * vector possible at all.
                 */
                auto armed = programmer.write_debug_register(kDemcr, kDemcrResetVectorCatch);

                if (!armed.ok())
                {
                    return armed.error().wrap(ErrorCode::TargetRefused,
                                              "arming the reset vector catch");
                }
            }

            auto asked = programmer.write_debug_register(kAircr, kAircrKey | kAircrSystemReset);

            if (!asked.ok())
            {
                return asked.error().wrap(ErrorCode::TargetRefused, "resetting through AIRCR");
            }

            std::this_thread::sleep_for(kResetSettle);

            if (halt_on_reset)
            {
                /*
                 * Disarmed once it has done its job. It survives nRST, so
                 * leaving it set would halt the board every time someone
                 * pressed its reset button long after we had gone.
                 */
                (void)programmer.write_debug_register(kDemcr, 0);
            }

            return {};
        }

        /**
         * @brief Reset, preferring the pin and falling back to software.
         *
         * S_RESET_ST is read first to clear it, then read again afterwards:
         * if it never set, the pulse on nRST reached nothing and the pin is
         * not wired, so the reset has to be asked for through AIRCR instead.
         */
        VoidResult reset_the_target(IProgrammer &programmer)
        {
            (void)saw_reset(programmer); /* Read to clear. */

            auto low = programmer.reset_pin(true);

            if (low.ok())
            {
                std::this_thread::sleep_for(kResetPulse);

                (void)programmer.reset_pin(false);
            }

            auto through_debug = programmer.reset();

            if (!through_debug.ok())
            {
                STLINK_LOG_DBG("the debug unit would not reset the target: %s",
                               through_debug.error().describe().c_str());
            }

            std::this_thread::sleep_for(kResetSettle);

            if (saw_reset(programmer))
            {
                return {};
            }

            STLINK_LOG_INF("nRST does not appear to be connected, resetting through AIRCR");

            return software_reset(programmer, false);
        }

        /**
         * @brief Hold the target still, attach, and catch it on the way out.
         *
         * The order matters and is not the obvious one. Debug is entered
         * before nRST is driven, not while it is held, because a target in
         * reset may not answer a debug port that is not yet up.
         */
        VoidResult connect_under_reset(IProgrammer &programmer)
        {
            auto low = programmer.reset_pin(true);

            if (!low.ok())
            {
                return low.error().wrap(ErrorCode::IO, "holding the target in reset");
            }

            /* Useful when nRST turns out not to be wired: it may halt anyway. */
            (void)programmer.halt();

            std::this_thread::sleep_for(kResetPulse);

            auto high = programmer.reset_pin(false);

            if (!high.ok())
            {
                return high.error().wrap(ErrorCode::IO, "releasing the target from reset");
            }

            /*
             * A race against the target's own startup. There is nothing to
             * wait on, so ask repeatedly until it stops or the window closes.
             */
            const auto until = std::chrono::steady_clock::now() + kCatchWindow;

            while (std::chrono::steady_clock::now() < until)
            {
                (void)programmer.halt();

                std::this_thread::sleep_for(kCatchInterval);
            }

            if (!saw_reset(programmer))
            {
                STLINK_LOG_WRN("nRST does not appear to be connected");
            }

            /*
             * Whatever happened above, this is what actually guarantees the
             * promise: reset again through AIRCR with the vector catch armed,
             * so the core stops before its first instruction.
             */
            return software_reset(programmer, true);
        }
    } // namespace

    VoidResult connect_to_target(IProgrammer &programmer, const DeviceOptions &options)
    {
        auto entered = programmer.enter_debug(options.debug);

        if (!entered.ok())
        {
            return entered.error().wrap(ErrorCode::TargetUnknown, "entering debug");
        }

        probe_access_port(programmer);

        switch (options.reset)
        {
        case ResetMode::HotPlug:
            /* Attached, and deliberately nothing else. */
            return {};

        case ResetMode::UnderReset:
            return connect_under_reset(programmer);

        case ResetMode::Normal:
        default:
            return reset_the_target(programmer);
        }
    }
} // namespace stlink
