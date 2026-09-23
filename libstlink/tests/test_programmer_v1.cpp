/**
 * @file    test_programmer_v1.cpp
 * @brief   The original command set, and the SCSI envelope it travels in.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Two things distinguish a V1 and both are checked here: the command
 * descriptor wrapped around every command, and the status wrapper that must be
 * read back afterwards. Everything else is the older opcode for an operation
 * the V2 tests already cover.
 */

#include <stlink/programmer_v1.h>

#include "programmer_fixture.h"

using namespace stlink;
using namespace stlink::testing_support;

namespace
{
    VersionReport a_v1(std::uint8_t jtag = 11)
    {
        VersionReport report;

        report.version.major = 1;
        report.version.jtag = jtag;
        report.api = ProtocolApi::V1;
        report.capabilities = Capability::None;
        report.max_trace_frequency = 0;

        return report;
    }

    using V1 = Fixture<ProgrammerV1>;

    /** @brief Where the command itself starts, past the descriptor. */
    constexpr std::size_t kAt = 15;
} // namespace

/* ---- The SCSI envelope ------------------------------------------------- */

TEST(ProgrammerV1, WrapsEveryCommandInAThirtyOneByteDescriptor)
{
    V1 f(a_v1());
    f.recorder->answers({0, 0});

    ASSERT_TRUE(f.programmer->reset().ok());

    const auto &block = f.recorder->sent[0];

    ASSERT_EQ(block.size(), 31u);
    EXPECT_EQ(block[0], 'U');
    EXPECT_EQ(block[1], 'S');
    EXPECT_EQ(block[2], 'B');
    EXPECT_EQ(block[3], 'C');

    /* The reply length it was told to expect, the direction, the unit, the length. */
    EXPECT_EQ(word_at(block, 8), 2u);
    EXPECT_EQ(block[12], 0x80); /* from the device */
    EXPECT_EQ(block[13], 0);
    EXPECT_EQ(block[14], 0x0a);

    /* And the command, after all of that. */
    EXPECT_EQ(block[kAt], 0xf2);
    EXPECT_EQ(block[kAt + 1], 0x03); /* ApiV1ResetSys */
}

TEST(ProgrammerV1, MarksADescriptorWhosePayloadTravelsToTheDevice)
{
    V1 f(a_v1());
    f.recorder->answers({0});

    const std::uint8_t data[4] = {1, 2, 3, 4};

    ASSERT_TRUE(f.programmer->write_memory(0x20000000, data, sizeof(data)).ok());

    EXPECT_EQ(f.recorder->sent[0][12], 0);
}

TEST(ProgrammerV1, ReadsTheStatusWrapperAfterEveryOperation)
{
    V1 f(a_v1());
    f.recorder->answers({0, 0});

    /* Three reads per operation would be wrong; it is the reply then the wrapper. */
    EXPECT_CALL(*f.transport, receive(::testing::_, 2u, ::testing::_)).Times(1);
    EXPECT_CALL(*f.transport, receive(::testing::_, 13u, ::testing::_)).Times(1);

    ASSERT_TRUE(f.programmer->reset().ok());
}

TEST(ProgrammerV1, ReadsTheWrapperAfterTheDataOfAWriteAndNotBeforeIt)
{
    V1 f(a_v1());
    f.recorder->answers({0});

    ::testing::InSequence ordered;

    EXPECT_CALL(*f.transport, send(::testing::_, 31u, ::testing::_));
    EXPECT_CALL(*f.transport, send(::testing::_, 4u, ::testing::_));
    EXPECT_CALL(*f.transport, receive(::testing::_, 13u, ::testing::_));

    const std::uint8_t data[4] = {1, 2, 3, 4};

    ASSERT_TRUE(f.programmer->write_memory(0x20000000, data, sizeof(data)).ok());
}

TEST(ProgrammerV1, MovesTheSequenceNumberOnAfterEachOperation)
{
    V1 f(a_v1());
    f.recorder->answers({0, 0});

    ASSERT_TRUE(f.programmer->reset().ok());
    ASSERT_TRUE(f.programmer->reset().ok());
    ASSERT_TRUE(f.programmer->reset().ok());

    EXPECT_EQ(word_at(f.recorder->sent[0], 4), 0u);
    EXPECT_EQ(word_at(f.recorder->sent[1], 4), 1u);
    EXPECT_EQ(word_at(f.recorder->sent[2], 4), 2u);
}

/* ---- The older opcodes, and the older reply layout --------------------- */

TEST(ProgrammerV1, ReadsTheCoreIdFromTheFrontOfTheReply)
{
    V1 f(a_v1());

    /* No status byte in front of it, unlike a V2. */
    f.recorder->answers({0x77, 0x14, 0xa0, 0x2b});

    auto id = f.programmer->core_id();

    ASSERT_TRUE(id.ok());
    EXPECT_EQ(id.value(), 0x2ba01477u);
    EXPECT_EQ(f.recorder->sent[0][kAt + 1], 0x22); /* ReadCoreId */
}

TEST(ProgrammerV1, EntersDebugWithTheOlderEnterCommand)
{
    V1 f(a_v1());
    f.recorder->answers({});

    ASSERT_TRUE(f.programmer->enter_debug(DebugMode::Swd).ok());

    EXPECT_EQ(f.recorder->sent[0][kAt + 1], 0x20); /* ApiV1Enter */
    EXPECT_EQ(f.recorder->sent[0][kAt + 2], 0xa3); /* EnterSwd */
}

TEST(ProgrammerV1, HaltsRunsAndStepsWithCommandsRatherThanDhcsr)
{
    V1 f(a_v1());
    f.recorder->answers({0, 0});

    ASSERT_TRUE(f.programmer->halt().ok());
    ASSERT_TRUE(f.programmer->run(false).ok());
    ASSERT_TRUE(f.programmer->step().ok());

    EXPECT_EQ(f.recorder->sent[0][kAt + 1], 0x02); /* ForceDebug */
    EXPECT_EQ(f.recorder->sent[1][kAt + 1], 0x09); /* RunCore */
    EXPECT_EQ(f.recorder->sent[2][kAt + 1], 0x0a); /* StepCore */
}

TEST(ProgrammerV1, ReadsTheStateFromTheStatusCommand)
{
    {
        V1 f(a_v1());
        f.recorder->answers({0x80, 0});
        EXPECT_EQ(f.programmer->state().value(), CoreState::Running);
        EXPECT_EQ(f.recorder->sent[0][kAt + 1], 0x01); /* GetStatus */
    }
    {
        V1 f(a_v1());
        f.recorder->answers({0x81, 0});
        EXPECT_EQ(f.programmer->state().value(), CoreState::Halted);
    }
    {
        V1 f(a_v1());
        f.recorder->answers({0x42, 0});
        EXPECT_EQ(f.programmer->state().value(), CoreState::Unknown);
    }
}

TEST(ProgrammerV1, ReadsOneRegisterWithNoAccessPortAndNoStatus)
{
    V1 f(a_v1());
    f.recorder->answers({0xef, 0xbe, 0xad, 0xde});

    auto value = f.programmer->read_register(3);

    ASSERT_TRUE(value.ok());
    EXPECT_EQ(value.value(), 0xdeadbeefu);
    EXPECT_EQ(f.recorder->sent[0][kAt + 1], 0x05); /* ApiV1ReadReg */
    EXPECT_EQ(f.recorder->sent[0][kAt + 2], 3);
}

TEST(ProgrammerV1, ReadsEveryRegisterFromTheFrontOfAnEightyFourByteReply)
{
    V1 f(a_v1());

    std::vector<std::uint8_t> reply(84, 0);
    reply[0] = 1;  /* r0 */
    reply[80] = 2; /* rw2, the last of them */

    f.recorder->answers(reply);

    auto registers = f.programmer->read_registers();

    ASSERT_TRUE(registers.ok());

    const CoreRegisters values = registers.value();

    EXPECT_EQ(values.r[0], 1u);
    EXPECT_EQ(values.rw2, 2u);
    EXPECT_EQ(f.recorder->sent[0][kAt + 1], 0x04); /* ApiV1ReadAllRegs */
}

TEST(ProgrammerV1, DoesNotConfirmAWriteBecauseItCannot)
{
    V1 f(a_v1());
    f.recorder->answers({0});

    const std::uint8_t data[4] = {1, 2, 3, 4};

    ASSERT_TRUE(f.programmer->write_memory(0x20000000, data, sizeof(data)).ok());

    /* The command and the data, and nothing asking whether it landed. */
    ASSERT_EQ(f.recorder->sent.size(), 2u);
}

TEST(ProgrammerV1, ReachesADebugRegisterAsMemory)
{
    V1 f(a_v1());
    f.recorder->answers({0x02, 0x00, 0x03, 0x00});

    auto value = f.programmer->read_debug_register(0xe000edf0);

    ASSERT_TRUE(value.ok());
    EXPECT_EQ(value.value(), 0x00030002u);
    EXPECT_EQ(f.recorder->sent[0][kAt + 1], 0x07); /* ReadMemory32Bit */
    EXPECT_EQ(word_at(f.recorder->sent[0], kAt + 2), 0xe000edf0u);
}

/* ---- What the original command set simply does not have ---------------- */

TEST(ProgrammerV1, RefusesWhatArrivedWithTheLaterCommandSet)
{
    V1 f(a_v1());

    EXPECT_EQ(f.programmer->set_clock(1000000).error().code(), ErrorCode::NotSupported);
    EXPECT_EQ(f.programmer->select_access_port(1).error().code(), ErrorCode::NotSupported);
    EXPECT_EQ(f.programmer->reset_pin(true).error().code(), ErrorCode::NotSupported);
    EXPECT_EQ(f.programmer->trace_enable(1000000).error().code(), ErrorCode::NotSupported);
    EXPECT_EQ(f.programmer->trace_disable().error().code(), ErrorCode::NotSupported);

    EXPECT_TRUE(f.recorder->sent.empty());
}

TEST(ProgrammerV1, OffersNoClockRatesAtAll)
{
    V1 f(a_v1());

    auto rates = f.programmer->clock_rates();

    ASSERT_TRUE(rates.ok());
    EXPECT_TRUE(rates.value().empty());
}

TEST(ProgrammerV1, CannotHoldATargetInReset)
{
    V1 f(a_v1());

    /*
     * What makes connecting under reset impossible on a V1 is that it cannot
     * drive nRST at all: the command arrived with the V2 set. The refusal is
     * here rather than in enter_debug, which now only enters debug.
     */
    auto held = f.programmer->reset_pin(true);

    ASSERT_FALSE(held.ok());
    EXPECT_EQ(held.error().code(), ErrorCode::NotSupported);
    EXPECT_TRUE(f.recorder->sent.empty());
}

TEST(ProgrammerV1, RefusesJtag)
{
    V1 f(a_v1());

    EXPECT_EQ(f.programmer->enter_debug(DebugMode::Jtag).error().code(),
              ErrorCode::NotSupported);
}

TEST(ProgrammerV1, StartsAFlashLoaderAnywayWithoutMaskingInterrupts)
{
    V1 f(a_v1());
    f.recorder->answers({0, 0});

    /* Refusing would leave no way to flash at all, so it runs and warns. */
    ASSERT_TRUE(f.programmer->run(true).ok());
    EXPECT_EQ(f.recorder->sent[0][kAt + 1], 0x09);
}
