/**
 * @file    test_chip_identity.cpp
 * @brief   Reading a target's identity, against a fake target.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What matters here is which address gets read, and in what order, because
 * that is the whole of the logic: the identity itself is whatever the target
 * hands back. So the fake records every address it was asked for, and the
 * tests assert on that sequence rather than only on the answer.
 */

#include <stlink/chip_identity.h>

#include <cstdint>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stlink/cortex.h>

#include "programmer_mock.h"

namespace stlink::testing_support
{
    namespace
    {
        constexpr std::uint32_t kIdcodeH7 = 0x5c001000;
        constexpr std::uint32_t kIdcodeCortexM0 = 0x40015800;
        constexpr std::uint32_t kIdcodeL5 = 0xe0044000;
        constexpr std::uint32_t kIdcodeH5 = 0x44024000;
        constexpr std::uint32_t kIdcodeDefault = 0xe0042000;
        constexpr std::uint32_t kJtagIdWb0 = 0x40000004;

        /** @brief The debug port an H7 and an H5 share. */
        constexpr std::uint32_t kCoreIdM7M33Swd = 0x6ba02477;

        TEST(ChipIdentity, DecodesEveryFieldOfCpuid)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            /* Implementer 0x41, variant 2, part 0xc24, revision 3. */
            target.at(kCpuid, 0x412fc243);

            auto read = read_cpu_id(programmer);

            ASSERT_TRUE(read.ok());

            const CpuId cpu = read.value();

            EXPECT_EQ(cpu.implementer, kImplementerArm);
            EXPECT_EQ(cpu.variant, 2);
            EXPECT_EQ(cpu.part, kPartCortexM4);
            EXPECT_EQ(cpu.revision, 3);
        }

        TEST(ChipIdentity, RefusesACoreThatIsNotArm)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            target.at(kCpuid, 0x00000000);

            auto read = read_cpu_id(programmer);

            ASSERT_FALSE(read.ok());
            EXPECT_EQ(read.error().code(), ErrorCode::TargetUnknown);
        }

        TEST(ChipIdentity, ACortexM4ReadsTheDefaultAddress)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            target.core(kPartCortexM4).at(kIdcodeDefault, 0x10006431);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x431u);
            EXPECT_THAT(target.read, ::testing::ElementsAre(kCpuid, kIdcodeDefault));
        }

        TEST(ChipIdentity, MasksTheRevisionOffTheIdentity)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            /* The top half is the revision, and is none of our business. */
            target.core(kPartCortexM3).at(kIdcodeDefault, 0xffff0410);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x410u);
        }

        TEST(ChipIdentity, ACortexM0ReadsItsOwnAddress)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            target.core(kPartCortexM0).at(kIdcodeCortexM0, 0x20000440);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x440u);
            EXPECT_THAT(target.read, ::testing::ElementsAre(kCpuid, kIdcodeCortexM0));
        }

        TEST(ChipIdentity, ACortexM0PlusFallsBackToJtagIdWhenThereIsNoIdcode)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            /*
             * A WB0 answers the usual address with zero rather than refusing,
             * and keeps its part number twelve bits up in JTAG_ID.
             */
            target.core(kPartCortexM0Plus).at(kJtagIdWb0, 0x01234567);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x234u);
            EXPECT_THAT(target.read,
                        ::testing::ElementsAre(kCpuid, kIdcodeCortexM0, kJtagIdWb0));
        }

        TEST(ChipIdentity, ACortexM33ReadsTheL5Address)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            target.core(kPartCortexM33).at(kIdcodeL5, 0x10000472);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x472u);
            EXPECT_THAT(target.read, ::testing::ElementsAre(kCpuid, kIdcodeL5));
        }

        TEST(ChipIdentity, ACortexM33FallsBackToTheH5Address)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            /* An H5 reads as zero where an L5 answers. */
            target.core(kPartCortexM33).at(kIdcodeH5, 0x10000484);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x484u);
            EXPECT_THAT(target.read, ::testing::ElementsAre(kCpuid, kIdcodeL5, kIdcodeH5));
        }

        TEST(ChipIdentity, AnH7IsACortexM7BehindTheSharedDebugPort)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            target.port(kCoreIdM7M33Swd).core(kPartCortexM7).at(kIdcodeH7, 0x10006450);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x450u);
            EXPECT_THAT(target.read, ::testing::ElementsAre(kCpuid, kIdcodeH7));
        }

        TEST(ChipIdentity, ACortexM7OnAnOrdinaryPortUsesTheDefaultAddress)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            /* An F7 is a Cortex-M7 too, and keeps its identity where everyone else does. */
            target.core(kPartCortexM7).at(kIdcodeDefault, 0x10000449);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x449u);
            EXPECT_THAT(target.read, ::testing::ElementsAre(kCpuid, kIdcodeDefault));
        }

        TEST(ChipIdentity, AppliesTheF4RevisionAErrata)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            target.core(kPartCortexM4).at(kIdcodeDefault, 0x10000411);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x413u);
        }

        TEST(ChipIdentity, LeavesAnF2Alone)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            /* The same identifier on a Cortex-M3 really is an F2. */
            target.core(kPartCortexM3).at(kIdcodeDefault, 0x10000411);

            auto read = read_chip_id(programmer);

            ASSERT_TRUE(read.ok());
            EXPECT_EQ(read.value(), 0x411u);
        }

        TEST(ChipIdentity, FailsWhenNothingAnswers)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            /* Every address reads as zero, including the fallback. */
            target.core(kPartCortexM0);

            auto read = read_chip_id(programmer);

            ASSERT_FALSE(read.ok());
            EXPECT_EQ(read.error().code(), ErrorCode::TargetUnknown);
        }

        TEST(ChipIdentity, CarriesTheChainWhenAReadFails)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            target.core(kPartCortexM4);

            ON_CALL(programmer, read_debug_register(kIdcodeDefault))
                .WillByDefault([](std::uint32_t) -> Result<std::uint32_t> {
                    return Error(ErrorCode::IO, "the probe stopped answering");
                });

            auto read = read_chip_id(programmer);

            ASSERT_FALSE(read.ok());
            EXPECT_EQ(read.error().code(), ErrorCode::TargetUnknown);
            EXPECT_EQ(read.error().root().code(), ErrorCode::IO);
        }

        TEST(ChipIdentity, FailsWhenTheDebugPortWillNotIdentifyItself)
        {
            NiceProgrammer programmer;
            FakeTarget target(programmer);

            ON_CALL(programmer, core_id()).WillByDefault([]() -> Result<std::uint32_t> {
                return Error(ErrorCode::Timeout, "the target did not answer");
            });

            auto read = read_chip_id(programmer);

            ASSERT_FALSE(read.ok());
            EXPECT_EQ(read.error().root().code(), ErrorCode::Timeout);
        }
    } // namespace
} // namespace stlink::testing_support
