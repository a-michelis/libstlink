/**
 * @file    test_command.cpp
 * @brief   Sending a command and reading its reply.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A mock transport stands in for the programmer, so the exchange, the status
 * handling and the retries can be tested with nothing attached.
 */

#include <stlink/command.h>

#include <cstring>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace stlink;

using ::testing::_;
using ::testing::Return;

namespace
{
    class MockTransport : public ITransport
    {
    public:
        MOCK_METHOD(const ProbeAddress &, address, (), (const, noexcept, override));
        MOCK_METHOD(Result<std::size_t>, send,
                    (const std::uint8_t *data, std::size_t size, Timeout timeout), (override));
        MOCK_METHOD(Result<std::size_t>, receive,
                    (std::uint8_t * data, std::size_t size, Timeout timeout), (override));
        MOCK_METHOD(Result<std::size_t>, receive_trace,
                    (std::uint8_t * data, std::size_t size, Timeout timeout), (override));
        MOCK_METHOD(VoidResult, reset, (Channel channel), (override));
    };

    /** @brief An action that fills the caller's buffer with a scripted reply. */
    auto Answers(std::vector<std::uint8_t> bytes)
    {
        return [captured = std::move(bytes)](std::uint8_t *data, std::size_t size,
                                             Timeout) -> Result<std::size_t> {
            const std::size_t n = (captured.size() < size) ? captured.size() : size;
            std::memcpy(data, captured.data(), n);

            return n;
        };
    }

    /** @brief An action that accepts the whole command. */
    auto Accepts()
    {
        return [](const std::uint8_t *, std::size_t size, Timeout) -> Result<std::size_t> {
            return size;
        };
    }

    constexpr std::uint8_t kCommand[16] = {};
} // namespace

TEST(CommandChannel, AcceptsAGoodStatus)
{
    auto transport = std::make_unique<MockTransport>();

    EXPECT_CALL(*transport, send(_, 16u, _)).WillOnce(Accepts());
    EXPECT_CALL(*transport, receive(_, 8u, _))
        .WillOnce(Answers({0x80, 1, 2, 3, 4, 5, 6, 7}));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply),
                                   Check::Status, "a test command");

    ASSERT_TRUE(result.ok()) << result.error().describe();
    EXPECT_EQ(result.value(), 8u);
    EXPECT_EQ(reply[1], 1);
}

TEST(CommandChannel, RefusesAFault)
{
    auto transport = std::make_unique<MockTransport>();

    EXPECT_CALL(*transport, send(_, _, _)).WillOnce(Accepts());
    EXPECT_CALL(*transport, receive(_, _, _))
        .WillOnce(Answers({static_cast<std::uint8_t>(CommandStatus::Fault)}));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply),
                                   Check::Status, "a test command");

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), ErrorCode::TargetRefused);
}

TEST(CommandChannel, RetriesWhileTheTargetAsksToWait)
{
    auto transport = std::make_unique<MockTransport>();

    /* Three attempts: the command goes again in full each time. */
    EXPECT_CALL(*transport, send(_, _, _)).Times(3).WillRepeatedly(Accepts());
    EXPECT_CALL(*transport, receive(_, _, _))
        .WillOnce(Answers({static_cast<std::uint8_t>(CommandStatus::ApWait)}))
        .WillOnce(Answers({static_cast<std::uint8_t>(CommandStatus::DpWait)}))
        .WillOnce(Answers({0x80, 9}));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply),
                                   Check::Retry, "a test command");

    EXPECT_TRUE(result.ok()) << result.error().describe();
}

TEST(CommandChannel, GivesUpWhenTheWaitNeverClears)
{
    auto transport = std::make_unique<MockTransport>();

    /* One attempt plus three retries, and then it stops. */
    EXPECT_CALL(*transport, send(_, _, _)).Times(4).WillRepeatedly(Accepts());
    EXPECT_CALL(*transport, receive(_, _, _))
        .Times(4)
        .WillRepeatedly(Answers({static_cast<std::uint8_t>(CommandStatus::ApWait)}));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply),
                                   Check::Retry, "a test command");

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), ErrorCode::TargetRefused);
}

TEST(CommandChannel, StatusAloneDoesNotRetry)
{
    auto transport = std::make_unique<MockTransport>();

    EXPECT_CALL(*transport, send(_, _, _)).Times(1).WillOnce(Accepts());
    EXPECT_CALL(*transport, receive(_, _, _))
        .WillOnce(Answers({static_cast<std::uint8_t>(CommandStatus::ApWait)}));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply),
                                   Check::Status, "a test command");

    EXPECT_FALSE(result.ok());
}

TEST(CommandChannel, RejectsAReplyOfTheWrongLength)
{
    auto transport = std::make_unique<MockTransport>();

    EXPECT_CALL(*transport, send(_, _, _)).WillOnce(Accepts());
    EXPECT_CALL(*transport, receive(_, _, _)).WillOnce(Answers({1, 2, 3}));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply),
                                   Check::ReplyLength, "a test command");

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), ErrorCode::Protocol);
}

TEST(CommandChannel, TakesAShortReplyWhenNothingIsInsistedOn)
{
    auto transport = std::make_unique<MockTransport>();

    EXPECT_CALL(*transport, send(_, _, _)).WillOnce(Accepts());
    EXPECT_CALL(*transport, receive(_, _, _)).WillOnce(Answers({1, 2, 3}));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply), Check::None,
                                   "a test command");

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), 3u);
}

TEST(CommandChannel, DoesNotReadWhenNoReplyIsExpected)
{
    auto transport = std::make_unique<MockTransport>();

    EXPECT_CALL(*transport, send(_, _, _)).WillOnce(Accepts());
    EXPECT_CALL(*transport, receive(_, _, _)).Times(0);

    CommandChannel channel(std::move(transport));

    auto result = channel.exchange(kCommand, sizeof(kCommand), nullptr, 0, Check::None,
                                   "a test command");

    EXPECT_TRUE(result.ok());
}

TEST(CommandChannel, CarriesTheContextOfAFailure)
{
    auto transport = std::make_unique<MockTransport>();

    EXPECT_CALL(*transport, send(_, _, _))
        .WillOnce(Return(Error(ErrorCode::IO, "the bus gave up")));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply),
                                   Check::Status, "reading the core id");

    ASSERT_FALSE(result.ok());

    const std::string described = result.error().describe();

    EXPECT_NE(described.find("reading the core id"), std::string::npos) << described;
    EXPECT_NE(described.find("the bus gave up"), std::string::npos) << described;
}

TEST(CommandChannel, WarnsButContinuesOnAPartialSend)
{
    auto transport = std::make_unique<MockTransport>();

    /* The programmer took half the command; the reply decides what that meant. */
    EXPECT_CALL(*transport, send(_, 16u, _)).WillOnce(Return(std::size_t{8}));
    EXPECT_CALL(*transport, receive(_, _, _)).WillOnce(Answers({0x80}));

    CommandChannel channel(std::move(transport));

    std::uint8_t reply[8] = {};
    auto result = channel.exchange(kCommand, sizeof(kCommand), reply, sizeof(reply),
                                   Check::Status, "a test command");

    EXPECT_TRUE(result.ok());
}
