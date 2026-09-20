/**
 * @file    test_programmer_v2.cpp
 * @brief   The V2 command set, against a mock transport.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What matters here is the bytes: which opcode, in which order, with the
 * access port where the firmware expects it. A port of a command set fails by
 * sending almost the right block, so almost is what these tests rule out.
 */

#include <stlink/cortex.h>
#include <stlink/programmer_v2.h>

#include "programmer_fixture.h"

using namespace stlink;
using namespace stlink::testing_support;

namespace
{
    VersionReport a_v2(std::uint8_t jtag = 30,
                       Capability capabilities = Capability::Trace | Capability::LastRwStatus2)
    {
        VersionReport report;

        report.version.major = 2;
        report.version.jtag = jtag;
        report.api = ProtocolApi::V2;
        report.capabilities = capabilities;
        report.max_trace_frequency = 2000000;

        return report;
    }

    using V2 = Fixture<ProgrammerV2>;
} // namespace

/* ---- Every block is the length the firmware reads ---------------------- */

TEST(ProgrammerV2, SendsSixteenByteCommandBlocks)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->reset().ok());

    ASSERT_EQ(f.recorder->sent.size(), 1u);
    EXPECT_EQ(f.recorder->sent[0].size(), 16u);
}

/* ---- The programmer itself --------------------------------------------- */

TEST(ProgrammerV2, AsksForTheCurrentModeAndNamesIt)
{
    V2 f(a_v2());
    f.recorder->answers({2, 0});

    auto mode = f.programmer->mode();

    ASSERT_TRUE(mode.ok());
    EXPECT_EQ(mode.value(), ProgrammerMode::Debug);
    EXPECT_EQ(f.recorder->sent[0][0], 0xf5);
}

TEST(ProgrammerV2, DoesNotInventAModeItDoesNotKnow)
{
    V2 f(a_v2());
    f.recorder->answers({9, 0});

    auto mode = f.programmer->mode();

    ASSERT_TRUE(mode.ok());
    EXPECT_EQ(mode.value(), ProgrammerMode::Unknown);
}

TEST(ProgrammerV2, TurnsTheTwoVoltageReadingsIntoMillivolts)
{
    V2 f(a_v2());

    /* Reference and measurement equal: the target sits at the 2.4V reference. */
    f.recorder->answers({0x10, 0, 0, 0, 0x10, 0, 0, 0});

    auto voltage = f.programmer->target_voltage();

    ASSERT_TRUE(voltage.ok());
    EXPECT_EQ(voltage.value(), 2400u);
    EXPECT_EQ(f.recorder->sent[0][0], 0xf7);
}

TEST(ProgrammerV2, SaysSoWhenTheVoltageCannotBeMeasured)
{
    V2 f(a_v2());
    f.recorder->answers({0, 0, 0, 0, 0, 0, 0, 0});

    EXPECT_FALSE(f.programmer->target_voltage().ok());
}

TEST(ProgrammerV2, RefusesAClockRateOnFirmwareTooOldToTakeOne)
{
    V2 f(a_v2(21));

    auto set = f.programmer->set_clock(1000000);

    ASSERT_FALSE(set.ok());
    EXPECT_EQ(set.error().code(), ErrorCode::NotSupported);
}

TEST(ProgrammerV2, OffersTheRatesItsFirmwareFixed)
{
    V2 f(a_v2());

    auto rates = f.programmer->clock_rates();

    ASSERT_TRUE(rates.ok());

    const std::vector<std::uint32_t> offered = rates.value();

    ASSERT_EQ(offered.size(), 12u);
    EXPECT_EQ(offered.front(), 4000000u);
    EXPECT_EQ(offered.back(), 5000u);

    /* Nothing was asked of the device to answer this. */
    EXPECT_TRUE(f.recorder->sent.empty());
}

TEST(ProgrammerV2, OffersNothingOnFirmwareTooOldToBeTold)
{
    V2 f(a_v2(21));

    auto rates = f.programmer->clock_rates();

    ASSERT_TRUE(rates.ok());
    EXPECT_TRUE(rates.value().empty());
}

TEST(ProgrammerV2, SendsTheDivisorForARateItOffers)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->set_clock(950000).ok());

    EXPECT_EQ(f.recorder->sent[0][0], 0xf2);
    EXPECT_EQ(f.recorder->sent[0][1], 0x43);
    EXPECT_EQ(f.recorder->sent[0][2], 3);
}

TEST(ProgrammerV2, TakesItsDefaultRateWhenAskedForNoneInParticular)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->set_clock(0).ok());

    /* 1.8MHz, whose divisor is 1. */
    EXPECT_EQ(f.recorder->sent[0][1], 0x43);
    EXPECT_EQ(f.recorder->sent[0][2], 1);
}

TEST(ProgrammerV2, RefusesARateItDoesNotOfferRatherThanRounding)
{
    V2 f(a_v2());

    auto set = f.programmer->set_clock(1000000);

    ASSERT_FALSE(set.ok());
    EXPECT_EQ(set.error().code(), ErrorCode::InvalidArgument);
    EXPECT_TRUE(f.recorder->sent.empty());
}

/* ---- Getting to a target ----------------------------------------------- */

TEST(ProgrammerV2, EntersDebugOverSwd)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->enter_debug(DebugMode::Swd, ResetMode::Normal).ok());

    EXPECT_EQ(f.recorder->sent[0][0], 0xf2);
    EXPECT_EQ(f.recorder->sent[0][1], 0x30); /* ApiV2Enter */
    EXPECT_EQ(f.recorder->sent[0][2], 0xa3); /* EnterSwd */
}

TEST(ProgrammerV2, HoldsAndReleasesResetWhenAskedToConnectUnderIt)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->enter_debug(DebugMode::Swd, ResetMode::UnderReset).ok());

    ASSERT_EQ(f.recorder->sent.size(), 3u);

    /* Held low, then entered, then released high. */
    EXPECT_EQ(f.recorder->sent[0][1], 0x3c);
    EXPECT_EQ(f.recorder->sent[0][2], 0x00);
    EXPECT_EQ(f.recorder->sent[1][1], 0x30);
    EXPECT_EQ(f.recorder->sent[2][1], 0x3c);
    EXPECT_EQ(f.recorder->sent[2][2], 0x01);
}

TEST(ProgrammerV2, RefusesJtagRatherThanSendingSomethingUnrecognised)
{
    V2 f(a_v2());

    auto entered = f.programmer->enter_debug(DebugMode::Jtag, ResetMode::Normal);

    ASSERT_FALSE(entered.ok());
    EXPECT_EQ(entered.error().code(), ErrorCode::NotSupported);
    EXPECT_TRUE(f.recorder->sent.empty());
}

TEST(ProgrammerV2, ReadsTheCoreIdFromFourBytesIn)
{
    V2 f(a_v2());
    f.recorder->answers(ok_with(0x2ba01477));

    auto id = f.programmer->core_id();

    ASSERT_TRUE(id.ok());
    EXPECT_EQ(id.value(), 0x2ba01477u);
    EXPECT_EQ(f.recorder->sent[0][1], 0x31);
}

TEST(ProgrammerV2, OnlyRemembersAnAccessPortTheFirmwareAccepted)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->select_access_port(1).ok());

    EXPECT_EQ(f.recorder->sent[0][1], 0x4b);
    EXPECT_EQ(f.recorder->sent[0][2], 1);

    /* Now that AP1 is selected, a debug register read goes through memory,
       which answers with the four bytes themselves and no status. */
    f.recorder->sent.clear();
    f.recorder->answers({1, 2, 3, 4});

    ASSERT_TRUE(f.programmer->read_debug_register(kDhcsr).ok());
    EXPECT_EQ(f.recorder->sent[0][1], 0x07); /* ReadMemory32Bit, not ReadDebugReg */
}

TEST(ProgrammerV2, DoesNotRememberAnAccessPortTheFirmwareRefused)
{
    V2 f(a_v2());
    f.recorder->answers({0x81, 0}); /* Fault */

    EXPECT_FALSE(f.programmer->select_access_port(1).ok());

    f.recorder->sent.clear();
    f.recorder->answers(ok_with(0));

    ASSERT_TRUE(f.programmer->read_debug_register(kDhcsr).ok());
    EXPECT_EQ(f.recorder->sent[0][1], 0x36); /* Still the native command. */
}

/* ---- Controlling the target, which is DHCSR and not a command ---------- */

TEST(ProgrammerV2, HaltsByWritingDhcsr)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->halt().ok());

    EXPECT_EQ(f.recorder->sent[0][1], 0x35); /* ApiV2WriteDebugReg */
    EXPECT_EQ(word_at(f.recorder->sent[0], 2), kDhcsr);
    EXPECT_EQ(word_at(f.recorder->sent[0], 6), kDhcsrKey | kDhcsrHalt | kDhcsrDebugEnable);
}

TEST(ProgrammerV2, RunsWithInterruptsMaskedOnlyForAFlashLoader)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->run(false).ok());
    EXPECT_EQ(word_at(f.recorder->sent[0], 6), kDhcsrKey | kDhcsrDebugEnable);

    f.recorder->sent.clear();

    ASSERT_TRUE(f.programmer->run(true).ok());
    EXPECT_EQ(word_at(f.recorder->sent[0], 6),
              kDhcsrKey | kDhcsrDebugEnable | kDhcsrMaskInterrupts);
}

TEST(ProgrammerV2, StepsInThreeWrites)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->step().ok());

    ASSERT_EQ(f.recorder->sent.size(), 3u);
    EXPECT_EQ(word_at(f.recorder->sent[0], 6),
              kDhcsrKey | kDhcsrHalt | kDhcsrMaskInterrupts | kDhcsrDebugEnable);
    EXPECT_EQ(word_at(f.recorder->sent[1], 6),
              kDhcsrKey | kDhcsrStep | kDhcsrMaskInterrupts | kDhcsrDebugEnable);
    EXPECT_EQ(word_at(f.recorder->sent[2], 6), kDhcsrKey | kDhcsrHalt | kDhcsrDebugEnable);
}

TEST(ProgrammerV2, ReadsTheStateOutOfDhcsr)
{
    {
        V2 f(a_v2());
        f.recorder->answers(ok_with(kDhcsrHalt));
        EXPECT_EQ(f.programmer->state().value(), CoreState::Halted);
    }
    {
        V2 f(a_v2());
        f.recorder->answers(ok_with(kDhcsrResetSince));
        EXPECT_EQ(f.programmer->state().value(), CoreState::Reset);
    }
    {
        V2 f(a_v2());
        f.recorder->answers(ok_with(0));
        EXPECT_EQ(f.programmer->state().value(), CoreState::Running);
    }
    {
        V2 f(a_v2());
        f.recorder->answers(ok_with(kDhcsrDebugEnable));
        EXPECT_EQ(f.programmer->state().value(), CoreState::DebugRunning);
    }
}

TEST(ProgrammerV2, DrivesTheResetPinBothWays)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->reset_pin(true).ok());
    EXPECT_EQ(f.recorder->sent[0][2], 0x00);

    f.recorder->sent.clear();

    ASSERT_TRUE(f.programmer->reset_pin(false).ok());
    EXPECT_EQ(f.recorder->sent[0][2], 0x01);
}

/* ---- Memory ------------------------------------------------------------- */

TEST(ProgrammerV2, ReadsMemoryWithTheAccessPortAfterTheLength)
{
    V2 f(a_v2());
    f.recorder->answers({1, 2, 3, 4});

    std::uint8_t into[4] = {};

    ASSERT_TRUE(f.programmer->read_memory(0x08000000, into, sizeof(into)).ok());

    const auto &block = f.recorder->sent[0];

    EXPECT_EQ(block[1], 0x07);
    EXPECT_EQ(word_at(block, 2), 0x08000000u);
    EXPECT_EQ(block[6] | (block[7] << 8), 4);
    EXPECT_EQ(block[8], 0); /* access port */
    EXPECT_EQ(into[0], 1);
}

TEST(ProgrammerV2, RefusesAnUnalignedRead)
{
    V2 f(a_v2());
    std::uint8_t into[4] = {};

    EXPECT_FALSE(f.programmer->read_memory(0x08000001, into, sizeof(into)).ok());
    EXPECT_FALSE(f.programmer->read_memory(0x08000000, into, 3).ok());
    EXPECT_TRUE(f.recorder->sent.empty());
}

TEST(ProgrammerV2, ReadingNothingIsNotAnError)
{
    V2 f(a_v2());

    EXPECT_TRUE(f.programmer->read_memory(0x08000000, nullptr, 0).ok());
    EXPECT_TRUE(f.recorder->sent.empty());
}

TEST(ProgrammerV2, AnAlignedWriteGoesAsWordsAndIsConfirmed)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

    const std::uint8_t data[4] = {1, 2, 3, 4};

    ASSERT_TRUE(f.programmer->write_memory(0x20000000, data, sizeof(data)).ok());

    /* The command, the payload, then the confirmation. */
    ASSERT_EQ(f.recorder->sent.size(), 3u);
    EXPECT_EQ(f.recorder->sent[0][1], 0x08);
    EXPECT_EQ(f.recorder->sent[1], std::vector<std::uint8_t>(data, data + 4));
    EXPECT_EQ(f.recorder->sent[2][1], 0x3e); /* GetLastRwStatus2 */
}

TEST(ProgrammerV2, AnUnalignedWriteGoesAsBytesAndIsNotConfirmed)
{
    V2 f(a_v2());

    const std::uint8_t data[3] = {1, 2, 3};

    ASSERT_TRUE(f.programmer->write_memory(0x20000001, data, sizeof(data)).ok());

    ASSERT_EQ(f.recorder->sent.size(), 2u);
    EXPECT_EQ(f.recorder->sent[0][1], 0x0d);
}

TEST(ProgrammerV2, AsksForTheOlderConfirmationWhenThatIsAllThereIs)
{
    V2 f(a_v2(14, Capability::None));
    f.recorder->answers({0x80, 0});

    const std::uint8_t data[4] = {1, 2, 3, 4};

    ASSERT_TRUE(f.programmer->write_memory(0x20000000, data, sizeof(data)).ok());

    EXPECT_EQ(f.recorder->sent[2][1], 0x3b); /* GetLastRwStatus */
}

TEST(ProgrammerV2, RefusesAByteWriteLongerThanOneCommandCarries)
{
    V2 f(a_v2());

    const std::vector<std::uint8_t> data(65, 0);

    EXPECT_FALSE(f.programmer->write_memory(0x20000001, data.data(), data.size()).ok());
    EXPECT_TRUE(f.recorder->sent.empty());
}

/* ---- Registers ---------------------------------------------------------- */

TEST(ProgrammerV2, ReadsOneRegisterWithTheAccessPortAfterTheIndex)
{
    V2 f(a_v2());
    f.recorder->answers(ok_with(0xdeadbeef));

    auto value = f.programmer->read_register(3);

    ASSERT_TRUE(value.ok());
    EXPECT_EQ(value.value(), 0xdeadbeefu);
    EXPECT_EQ(f.recorder->sent[0][1], 0x33);
    EXPECT_EQ(f.recorder->sent[0][2], 3);
    EXPECT_EQ(f.recorder->sent[0][3], 0);
}

TEST(ProgrammerV2, WritesOneRegisterWithTheAccessPortAfterTheValue)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->write_register(3, 0x12345678).ok());

    EXPECT_EQ(f.recorder->sent[0][1], 0x34);
    EXPECT_EQ(f.recorder->sent[0][2], 3);
    EXPECT_EQ(word_at(f.recorder->sent[0], 3), 0x12345678u);
    EXPECT_EQ(f.recorder->sent[0][7], 0);
}

TEST(ProgrammerV2, ReadsEveryRegisterFromFourBytesIn)
{
    V2 f(a_v2());

    std::vector<std::uint8_t> reply(88, 0);
    reply[0] = 0x80;

    /* r0 = 1, and rw2, the last of them, = 2. */
    reply[4] = 1;
    reply[4 + 80] = 2;

    f.recorder->answers(reply);

    auto registers = f.programmer->read_registers();

    ASSERT_TRUE(registers.ok());

    const CoreRegisters values = registers.value();

    EXPECT_EQ(values.r[0], 1u);
    EXPECT_EQ(values.rw2, 2u);
    EXPECT_EQ(f.recorder->sent[0][1], 0x3a);
}

/* ---- Trace -------------------------------------------------------------- */

TEST(ProgrammerV2, RefusesTraceOnFirmwareThatHasNone)
{
    V2 f(a_v2(12, Capability::None));

    auto enabled = f.programmer->trace_enable(1000000);

    ASSERT_FALSE(enabled.ok());
    EXPECT_EQ(enabled.error().code(), ErrorCode::NotSupported);
}

TEST(ProgrammerV2, RefusesTraceFasterThanItCanCarry)
{
    V2 f(a_v2());

    EXPECT_FALSE(f.programmer->trace_enable(24000000).ok());
    EXPECT_TRUE(f.recorder->sent.empty());
}

TEST(ProgrammerV2, StartsTraceWithTwiceItsBufferAndTheRate)
{
    V2 f(a_v2());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->trace_enable(2000000).ok());

    const auto &block = f.recorder->sent[0];

    EXPECT_EQ(block[1], 0x40);
    EXPECT_EQ(block[2] | (block[3] << 8), 4096); /* 2 * 2048 */
    EXPECT_EQ(word_at(block, 4), 2000000u);
}

TEST(ProgrammerV2, ReadsNothingWhenNoTraceIsWaiting)
{
    V2 f(a_v2());
    f.recorder->answers({0, 0});

    std::uint8_t into[16] = {};

    auto read = f.programmer->trace_read(into, sizeof(into));

    ASSERT_TRUE(read.ok());
    EXPECT_EQ(read.value(), 0u);
}

TEST(ProgrammerV2, RefusesToReadTraceIntoTooSmallABuffer)
{
    V2 f(a_v2());
    f.recorder->answers({64, 0}); /* 64 bytes waiting */

    std::uint8_t into[16] = {};

    EXPECT_FALSE(f.programmer->trace_read(into, sizeof(into)).ok());
}
