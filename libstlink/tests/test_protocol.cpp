/**
 * @file    test_protocol.cpp
 * @brief   Product ids, and decoding the version reply.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The version reply is the one exchange that can be decoded without a
 * connection, so these are byte-for-byte tests against replies real
 * programmers send.
 */

#include <stlink/protocol.h>

#include <iterator>
#include <vector>

#include <gtest/gtest.h>

using namespace stlink;

namespace
{
    /**
     * @brief Build the packed six byte reply a V1 or V2 sends.
     *
     * Packing it here rather than writing the bytes out by hand keeps the
     * tests about what the fields mean; the packing itself is checked once,
     * against a literal, in ReadsAPackedReplyByHand below.
     */
    std::vector<std::uint8_t> packed(std::uint8_t major, std::uint8_t jtag, std::uint8_t swim,
                                     std::uint16_t vid = 0x0483, std::uint16_t pid = 0x3748)
    {
        return {
            static_cast<std::uint8_t>((major << 4) | ((jtag >> 2) & 0x0f)),
            static_cast<std::uint8_t>(((jtag & 0x03) << 6) | (swim & 0x3f)),
            static_cast<std::uint8_t>(vid & 0xff),
            static_cast<std::uint8_t>(vid >> 8),
            static_cast<std::uint8_t>(pid & 0xff),
            static_cast<std::uint8_t>(pid >> 8),
        };
    }

    VersionReport decode(Generation generation, const std::vector<std::uint8_t> &reply)
    {
        auto parsed = parse_version(generation, reply.data(), reply.size());

        EXPECT_TRUE(parsed.ok());

        return parsed.value();
    }
} // namespace

/* ---- Product ids ------------------------------------------------------ */

TEST(Protocol, PlacesEveryShippedProductId)
{
    EXPECT_EQ(generation_of(0x3744), Generation::V1);

    EXPECT_EQ(generation_of(0x3748), Generation::V2);
    EXPECT_EQ(generation_of(0x374a), Generation::V2);
    EXPECT_EQ(generation_of(0x374b), Generation::V2);

    EXPECT_EQ(generation_of(0x3752), Generation::V2_1);

    EXPECT_EQ(generation_of(0x374d), Generation::V3);
    EXPECT_EQ(generation_of(0x374e), Generation::V3);
    EXPECT_EQ(generation_of(0x374f), Generation::V3);
    EXPECT_EQ(generation_of(0x3753), Generation::V3);
    EXPECT_EQ(generation_of(0x3754), Generation::V3);
    EXPECT_EQ(generation_of(0x3757), Generation::V3);
}

TEST(Protocol, DoesNotClaimSomethingElsesProductId)
{
    EXPECT_EQ(generation_of(0x0000), Generation::Unknown);
    EXPECT_EQ(generation_of(0x3745), Generation::Unknown);
    EXPECT_EQ(generation_of(0xffff), Generation::Unknown);
}

TEST(Protocol, RecognisesAProgrammerOnlyUnderStsVendorId)
{
    EXPECT_TRUE(is_programmer(0x0483, 0x3748));
    EXPECT_FALSE(is_programmer(0x0484, 0x3748));
    EXPECT_FALSE(is_programmer(0x0483, 0x1234));
}

TEST(Protocol, AsksForTheReplyLengthItsGenerationSends)
{
    EXPECT_EQ(version_reply_size(Generation::V1), 6u);
    EXPECT_EQ(version_reply_size(Generation::V2), 6u);
    EXPECT_EQ(version_reply_size(Generation::V2_1), 6u);
    EXPECT_EQ(version_reply_size(Generation::V3), 12u);
}

/* ---- The packed reply, before V3 -------------------------------------- */

TEST(Protocol, ReadsAPackedReplyByHand)
{
    /* major 2, jtag 29, swim 7, vid 0x0483, pid 0x3748. */
    const std::uint8_t reply[] = {0x27, 0x47, 0x83, 0x04, 0x48, 0x37};

    const VersionReport report = decode(Generation::V2, {std::begin(reply), std::end(reply)});

    EXPECT_EQ(report.version.major, 2);
    EXPECT_EQ(report.version.jtag, 29);
    EXPECT_EQ(report.version.swim, 7);
    EXPECT_EQ(report.version.vid, 0x0483);
    EXPECT_EQ(report.version.pid, 0x3748);
}

TEST(Protocol, TheRevisionsSurviveTheirPacking)
{
    /* jtag is six bits split across two bytes, so walk its whole range. */
    for (std::uint8_t jtag = 0; jtag < 64; ++jtag)
    {
        const VersionReport report = decode(Generation::V2, packed(2, jtag, 0x2a));

        EXPECT_EQ(report.version.jtag, jtag);
        EXPECT_EQ(report.version.swim, 0x2a);
        EXPECT_EQ(report.version.major, 2);
    }
}

/* ---- What a revision is worth ----------------------------------------- */

TEST(Protocol, AV2GainsTraceAtJ13)
{
    EXPECT_FALSE(has_capability(decode(Generation::V2, packed(2, 12, 0)).capabilities,
                                Capability::Trace));
    EXPECT_TRUE(has_capability(decode(Generation::V2, packed(2, 13, 0)).capabilities,
                               Capability::Trace));

    EXPECT_EQ(decode(Generation::V2, packed(2, 12, 0)).max_trace_frequency, 0u);
    EXPECT_EQ(decode(Generation::V2, packed(2, 13, 0)).max_trace_frequency, 2000000u);
}

TEST(Protocol, AV2GainsTheBetterStatusCommandAtJ15)
{
    EXPECT_FALSE(has_capability(decode(Generation::V2, packed(2, 14, 0)).capabilities,
                                Capability::LastRwStatus2));
    EXPECT_TRUE(has_capability(decode(Generation::V2, packed(2, 15, 0)).capabilities,
                               Capability::LastRwStatus2));
}

TEST(Protocol, AV1SpeaksTheOlderCommandSetUntilJ12)
{
    EXPECT_EQ(decode(Generation::V1, packed(1, 11, 0)).api, ProtocolApi::V1);
    EXPECT_EQ(decode(Generation::V1, packed(1, 12, 0)).api, ProtocolApi::V2);
}

TEST(Protocol, AV1CarriesNoTraceHoweverNewItIs)
{
    const VersionReport report = decode(Generation::V1, packed(1, 63, 0));

    EXPECT_FALSE(has_capability(report.capabilities, Capability::Trace));
    EXPECT_EQ(report.max_trace_frequency, 0u);
}

/* ---- The V3 reply ----------------------------------------------------- */

TEST(Protocol, ReadsTheV3Reply)
{
    /* A byte each, and the ids eight bytes in. */
    const std::uint8_t reply[] = {3, 0, 8, 0, 0, 0, 0, 0, 0x83, 0x04, 0x4e, 0x37};

    const VersionReport report = decode(Generation::V3, {std::begin(reply), std::end(reply)});

    EXPECT_EQ(report.version.major, 3);
    EXPECT_EQ(report.version.swim, 0);
    EXPECT_EQ(report.version.jtag, 8);
    EXPECT_EQ(report.version.vid, 0x0483);
    EXPECT_EQ(report.version.pid, 0x374e);
    EXPECT_EQ(report.api, ProtocolApi::V3);
}

TEST(Protocol, AV3HasEverythingAtEveryRevision)
{
    const std::uint8_t reply[] = {3, 0, 1, 0, 0, 0, 0, 0, 0x83, 0x04, 0x4e, 0x37};

    const VersionReport report = decode(Generation::V3, {std::begin(reply), std::end(reply)});

    EXPECT_TRUE(has_capability(report.capabilities, Capability::Trace));
    EXPECT_TRUE(has_capability(report.capabilities, Capability::LastRwStatus2));
    EXPECT_EQ(report.max_trace_frequency, 24000000u);
}

/* ---- Replies that are not replies -------------------------------------- */

TEST(Protocol, RefusesAReplyThatIsTooShort)
{
    const std::uint8_t reply[] = {0x27, 0x47, 0x83, 0x04, 0x48};

    auto parsed = parse_version(Generation::V2, reply, sizeof(reply));

    ASSERT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.error().code(), ErrorCode::Protocol);
}

TEST(Protocol, RefusesASixByteReplyFromAV3)
{
    const std::uint8_t reply[] = {0x27, 0x47, 0x83, 0x04, 0x48, 0x37};

    EXPECT_FALSE(parse_version(Generation::V3, reply, sizeof(reply)).ok());
}

TEST(Protocol, RefusesNoReplyAtAll)
{
    EXPECT_FALSE(parse_version(Generation::V2, nullptr, 0).ok());
}
