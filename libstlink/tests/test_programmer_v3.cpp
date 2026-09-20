/**
 * @file    test_programmer_v3.cpp
 * @brief   The four places a V3 differs from a V2.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Everything else a V3 does is inherited and already covered by the V2 tests,
 * so what is here is the difference and nothing more: the clock negotiation,
 * how much an unaligned write may carry, and how much trace is buffered. The
 * fourth, the version command, is settled by the factory before this class
 * exists and is tested with the version decode.
 */

#include <stlink/programmer_v3.h>

#include "programmer_fixture.h"

using namespace stlink;
using namespace stlink::testing_support;

namespace
{
    VersionReport a_v3()
    {
        VersionReport report;

        report.version.major = 3;
        report.version.jtag = 8;
        report.api = ProtocolApi::V3;
        report.capabilities = Capability::Trace | Capability::LastRwStatus2;
        report.max_trace_frequency = 24000000;

        return report;
    }

    /** @brief The reply to a clock rate enquiry, listing @p rates in kHz. */
    std::vector<std::uint8_t> offers(const std::vector<std::uint32_t> &rates)
    {
        std::vector<std::uint8_t> reply(52, 0);

        reply[0] = 0x80;
        reply[8] = static_cast<std::uint8_t>(rates.size());

        for (std::size_t i = 0; i < rates.size(); ++i)
        {
            const std::uint32_t rate = rates[i];

            reply[12 + i * 4 + 0] = static_cast<std::uint8_t>(rate & 0xff);
            reply[12 + i * 4 + 1] = static_cast<std::uint8_t>((rate >> 8) & 0xff);
            reply[12 + i * 4 + 2] = static_cast<std::uint8_t>((rate >> 16) & 0xff);
            reply[12 + i * 4 + 3] = static_cast<std::uint8_t>((rate >> 24) & 0xff);
        }

        return reply;
    }

    using V3 = Fixture<ProgrammerV3>;
} // namespace

/* ---- The clock is read from the device, then set exactly --------------- */

TEST(ProgrammerV3, ReportsTheRatesTheDeviceListsInHertz)
{
    V3 f(a_v3());
    f.recorder->answers(offers({24000, 8000, 3300, 1000}));

    auto rates = f.programmer->clock_rates();

    ASSERT_TRUE(rates.ok());
    EXPECT_EQ(rates.value(),
              (std::vector<std::uint32_t>{24000000, 8000000, 3300000, 1000000}));

    /* It had to ask, for SWD. */
    EXPECT_EQ(f.recorder->sent[0][1], 0x62);
    EXPECT_EQ(f.recorder->sent[0][2], 0);
}

TEST(ProgrammerV3, TreatsAZeroInTheListAsPaddingAndNotARate)
{
    V3 f(a_v3());
    f.recorder->answers(offers({24000, 0, 1000}));

    auto rates = f.programmer->clock_rates();

    ASSERT_TRUE(rates.ok());
    EXPECT_EQ(rates.value(), (std::vector<std::uint32_t>{24000000, 1000000}));
}

TEST(ProgrammerV3, ReadsNoMoreRatesThanTheReplyHoldsHoweverManyAreClaimed)
{
    V3 f(a_v3());

    auto reply = offers({24000, 8000, 3300, 1000});
    reply[8] = 99; /* More than the reply could possibly carry. */

    f.recorder->answers(reply);

    auto rates = f.programmer->clock_rates();

    ASSERT_TRUE(rates.ok());
    EXPECT_LE(rates.value().size(), 10u);
}

TEST(ProgrammerV3, SetsARateItListedInTheUnitItListedIt)
{
    V3 f(a_v3());
    f.recorder->answers(offers({24000, 8000, 3300, 1000}));

    ASSERT_TRUE(f.programmer->set_clock(8000000).ok());

    ASSERT_EQ(f.recorder->sent.size(), 2u);
    EXPECT_EQ(f.recorder->sent[1][1], 0x61);
    EXPECT_EQ(f.recorder->sent[1][2], 0);
    EXPECT_EQ(word_at(f.recorder->sent[1], 4), 8000u); /* kilohertz, as listed */
}

TEST(ProgrammerV3, TakesItsDefaultRateWhenAskedForNoneInParticular)
{
    V3 f(a_v3());
    f.recorder->answers(offers({24000, 8000, 1000, 200}));

    ASSERT_TRUE(f.programmer->set_clock(0).ok());

    EXPECT_EQ(word_at(f.recorder->sent[1], 4), 1000u);
}

TEST(ProgrammerV3, RefusesARateTheDeviceDidNotListRatherThanRounding)
{
    V3 f(a_v3());
    f.recorder->answers(offers({24000, 8000, 3300, 1000}));

    auto set = f.programmer->set_clock(5000000);

    ASSERT_FALSE(set.ok());
    EXPECT_EQ(set.error().code(), ErrorCode::InvalidArgument);

    /* It asked, and then sent nothing. */
    EXPECT_EQ(f.recorder->sent.size(), 1u);
}

TEST(ProgrammerV3, RefusesWhenTheProgrammerOffersNothing)
{
    V3 f(a_v3());
    f.recorder->answers(offers({}));

    auto set = f.programmer->set_clock(1000000);

    ASSERT_FALSE(set.ok());
    EXPECT_EQ(set.error().code(), ErrorCode::Protocol);
    EXPECT_EQ(f.recorder->sent.size(), 1u);
}

TEST(ProgrammerV3, DoesNotUseTheV2ClockPath)
{
    V3 f(a_v3());
    f.recorder->answers(offers({1000}));

    ASSERT_TRUE(f.programmer->set_clock(1000000).ok());

    /* A V2 would have sent SWD_SET_FREQ, and its firmware check would have
       refused a major version of 3 outright. */
    EXPECT_NE(f.recorder->sent[0][1], 0x43);
}

/* ---- It carries more than a V2 ----------------------------------------- */

TEST(ProgrammerV3, TakesAnUnalignedWriteAV2WouldRefuse)
{
    V3 f(a_v3());
    f.recorder->answers({0x80, 0});

    const std::vector<std::uint8_t> data(512, 0xa5);

    ASSERT_TRUE(f.programmer->write_memory(0x20000001, data.data(), data.size()).ok());

    EXPECT_EQ(f.recorder->sent[0][1], 0x0d); /* WriteMemory8Bit */
    EXPECT_EQ(f.recorder->sent[1].size(), 512u);
}

TEST(ProgrammerV3, StillRefusesAnUnalignedWriteBeyondItsOwnLimit)
{
    V3 f(a_v3());

    const std::vector<std::uint8_t> data(513, 0);

    EXPECT_FALSE(f.programmer->write_memory(0x20000001, data.data(), data.size()).ok());
    EXPECT_TRUE(f.recorder->sent.empty());
}

TEST(ProgrammerV3, StartsTraceWithTwiceItsLargerBuffer)
{
    V3 f(a_v3());
    f.recorder->answers({0x80, 0});

    ASSERT_TRUE(f.programmer->trace_enable(2000000).ok());

    const auto &block = f.recorder->sent[0];

    EXPECT_EQ(block[1], 0x40);
    EXPECT_EQ(block[2] | (block[3] << 8), 16384); /* 2 * 8192 */
}

TEST(ProgrammerV3, CarriesTraceAV2CouldNot)
{
    V3 f(a_v3());
    f.recorder->answers({0x80, 0});

    /* 24MHz would be refused by a V2, whose ceiling is 2MHz. */
    EXPECT_TRUE(f.programmer->trace_enable(24000000).ok());
}
