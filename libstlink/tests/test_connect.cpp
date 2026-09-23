/**
 * @file    test_connect.cpp
 * @brief   The three connect modes, and what each one does to the target.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What is worth asserting here is the sequence, not the return value. All
 * three modes end with a debug connection; they differ entirely in what
 * happened to the target on the way, and a mode that quietly did nothing
 * would pass any test that only checked ok().
 */

#include <stlink/connect.h>

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stlink/cortex.h>

#include "programmer_mock.h"

namespace stlink::testing_support
{
    namespace
    {
        using ::testing::_;

        /** @brief Records what was done to the target, in order. */
        class Journal
        {
        public:
            explicit Journal(NiceProgrammer &programmer)
            {
                ON_CALL(programmer, enter_debug(_)).WillByDefault([this](DebugMode) {
                    entries.emplace_back("enter");

                    return VoidResult{};
                });

                ON_CALL(programmer, reset_pin(_)).WillByDefault([this](bool asserted) {
                    entries.emplace_back(asserted ? "nrst low" : "nrst high");

                    return VoidResult{};
                });

                ON_CALL(programmer, halt()).WillByDefault([this] {
                    halts++;

                    return VoidResult{};
                });

                ON_CALL(programmer, reset()).WillByDefault([this] {
                    entries.emplace_back("debug reset");

                    return VoidResult{};
                });

                ON_CALL(programmer, select_access_port(_)).WillByDefault([this](std::uint8_t ap) {
                    entries.emplace_back("select ap " + std::to_string(ap));

                    return VoidResult{};
                });

                ON_CALL(programmer, write_debug_register(_, _))
                    .WillByDefault([this](std::uint32_t address, std::uint32_t value) {
                        if (address == kAircr)
                        {
                            entries.emplace_back("aircr reset");
                        }
                        else if (address == kDemcr)
                        {
                            entries.emplace_back(value == 0 ? "vector catch off"
                                                            : "vector catch on");
                        }

                        return VoidResult{};
                    });
            }

            [[nodiscard]] bool did(const std::string &what) const
            {
                for (const auto &entry : entries)
                {
                    if (entry == what)
                    {
                        return true;
                    }
                }

                return false;
            }

            std::vector<std::string> entries;
            int halts = 0;
        };

        /** @brief A target that is present, answers as ARM, and reports a reset. */
        void a_healthy_target(NiceProgrammer &programmer, FakeTarget &target)
        {
            target.core(kPartCortexM4);
            target.at(kDhcsr, kDhcsrResetSince);

            ON_CALL(programmer, select_access_port(_)).WillByDefault([](std::uint8_t) {
                return VoidResult{};
            });
        }

        TEST(Connect, HotPlugAttachesAndLeavesTheTargetAlone)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);
            Journal journal(programmer);

            target.core(kPartCortexM4);

            DeviceOptions options;
            options.reset = ResetMode::HotPlug;

            ASSERT_TRUE(connect_to_target(programmer, options).ok());

            EXPECT_TRUE(journal.did("enter"));

            /* Nothing that would disturb a running target. */
            EXPECT_FALSE(journal.did("nrst low"));
            EXPECT_FALSE(journal.did("debug reset"));
            EXPECT_FALSE(journal.did("aircr reset"));
            EXPECT_EQ(journal.halts, 0);
        }

        TEST(Connect, NormalResetsTheTarget)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);
            Journal journal(programmer);

            a_healthy_target(programmer, target);

            DeviceOptions options;
            options.reset = ResetMode::Normal;

            ASSERT_TRUE(connect_to_target(programmer, options).ok());

            EXPECT_TRUE(journal.did("enter"));
            EXPECT_TRUE(journal.did("nrst low"));
            EXPECT_TRUE(journal.did("nrst high"));

            /* nRST was seen to work, so no software fallback was needed. */
            EXPECT_FALSE(journal.did("aircr reset"));
        }

        TEST(Connect, NormalFallsBackToSoftwareResetWhenNrstIsNotWired)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);
            Journal journal(programmer);

            target.core(kPartCortexM4);

            /* DHCSR never reports a reset, so the pin reached nothing. */
            target.at(kDhcsr, 0);

            DeviceOptions options;
            options.reset = ResetMode::Normal;

            ASSERT_TRUE(connect_to_target(programmer, options).ok());

            EXPECT_TRUE(journal.did("nrst low"));
            EXPECT_TRUE(journal.did("aircr reset"));
        }

        TEST(Connect, UnderResetCatchesTheCoreAtItsResetVector)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);
            Journal journal(programmer);

            a_healthy_target(programmer, target);

            DeviceOptions options;
            options.reset = ResetMode::UnderReset;

            ASSERT_TRUE(connect_to_target(programmer, options).ok());

            EXPECT_TRUE(journal.did("enter"));
            EXPECT_TRUE(journal.did("nrst low"));
            EXPECT_TRUE(journal.did("nrst high"));

            /* The point of the mode: it must not be left running. */
            EXPECT_GT(journal.halts, 1);
            EXPECT_TRUE(journal.did("vector catch on"));
            EXPECT_TRUE(journal.did("aircr reset"));

            /* And disarmed, or the board halts on every later reset. */
            EXPECT_TRUE(journal.did("vector catch off"));
        }

        TEST(Connect, EntersDebugBeforeDrivingReset)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);
            Journal journal(programmer);

            a_healthy_target(programmer, target);

            DeviceOptions options;
            options.reset = ResetMode::UnderReset;

            ASSERT_TRUE(connect_to_target(programmer, options).ok());

            /*
             * A target held in reset may not answer a debug port that is not
             * up yet, so the order is attach first, then drive the pin.
             */
            ASSERT_FALSE(journal.entries.empty());
            EXPECT_EQ(journal.entries.front(), "enter");
        }

        TEST(Connect, LooksForTheCpuOnPortOneWhenTheDefaultHasNone)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);
            Journal journal(programmer);

            /*
             * CPUID reads as zero, which names no ARM core, so the CPU is not
             * on the default access port. An H5 is the part this is for.
             */
            target.at(kCpuid, 0);

            DeviceOptions options;
            options.reset = ResetMode::HotPlug;

            ASSERT_TRUE(connect_to_target(programmer, options).ok());

            EXPECT_TRUE(journal.did("select ap 1"));

            /* Nothing there either, so the default is restored. */
            EXPECT_EQ(journal.entries.back(), "select ap 0");
        }

        TEST(Connect, LeavesTheAccessPortAloneWhenTheCpuIsOnTheDefault)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);
            Journal journal(programmer);

            target.core(kPartCortexM7);

            DeviceOptions options;
            options.reset = ResetMode::HotPlug;

            ASSERT_TRUE(connect_to_target(programmer, options).ok());

            EXPECT_FALSE(journal.did("select ap 1"));
        }

        TEST(Connect, FailsWhenDebugCannotBeEntered)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            ON_CALL(programmer, enter_debug(_)).WillByDefault([](DebugMode) {
                return VoidResult(Error(ErrorCode::NotSupported, "entering debug over JTAG"));
            });

            DeviceOptions options;
            options.debug = DebugMode::Jtag;

            auto connected = connect_to_target(programmer, options);

            ASSERT_FALSE(connected.ok());
            EXPECT_EQ(connected.error().root().code(), ErrorCode::NotSupported);
        }
    } // namespace
} // namespace stlink::testing_support
