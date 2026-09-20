/**
 * @file    programmer_fixture.h
 * @brief   A mock transport that records what a programmer sends.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Shared by the tests for each command set, since what they check is the same
 * thing: the exact bytes that went out, and what the programmer made of what
 * came back.
 */

#ifndef STLINK_TESTS_PROGRAMMER_FIXTURE_H
#define STLINK_TESTS_PROGRAMMER_FIXTURE_H

#include <stlink/programmer_base.h>

#include <cstring>
#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace stlink::testing_support
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

    using NiceTransport = ::testing::NiceMock<MockTransport>;

    /** @brief Remembers every block sent, and answers reads from a script. */
    class Recorder
    {
    public:
        explicit Recorder(NiceTransport &transport) : transport_(transport)
        {
            ON_CALL(transport_, send(::testing::_, ::testing::_, ::testing::_))
                .WillByDefault([this](const std::uint8_t *data, std::size_t size, Timeout) {
                    sent.emplace_back(data, data + size);

                    return Result<std::size_t>(size);
                });
        }

        /** @brief Answer every read, and every trace read, with these bytes. */
        void answers(std::vector<std::uint8_t> bytes)
        {
            auto reply = [captured = std::move(bytes)](std::uint8_t *data, std::size_t size,
                                                       Timeout) {
                const std::size_t n = captured.size() < size ? captured.size() : size;

                std::memcpy(data, captured.data(), n);

                return Result<std::size_t>(n);
            };

            ON_CALL(transport_, receive(::testing::_, ::testing::_, ::testing::_))
                .WillByDefault(reply);
            ON_CALL(transport_, receive_trace(::testing::_, ::testing::_, ::testing::_))
                .WillByDefault(reply);
        }

        std::vector<std::vector<std::uint8_t>> sent;

    private:
        NiceTransport &transport_;
    };

    /** @brief A reply that says Ok and carries @p value four bytes in. */
    inline std::vector<std::uint8_t> ok_with(std::uint32_t value)
    {
        return {0x80, 0, 0, 0,
                static_cast<std::uint8_t>(value & 0xff),
                static_cast<std::uint8_t>((value >> 8) & 0xff),
                static_cast<std::uint8_t>((value >> 16) & 0xff),
                static_cast<std::uint8_t>((value >> 24) & 0xff)};
    }

    /** @brief The little endian word starting at @p at of a recorded block. */
    inline std::uint32_t word_at(const std::vector<std::uint8_t> &block, std::size_t at)
    {
        return block[at] | (block[at + 1] << 8) | (block[at + 2] << 16) |
               (static_cast<std::uint32_t>(block[at + 3]) << 24);
    }

    /** @brief A programmer of type @p T, and the recorder watching it. */
    template <typename T>
    struct Fixture
    {
        explicit Fixture(VersionReport report)
        {
            auto owned = std::make_unique<NiceTransport>();

            transport = owned.get();
            recorder = std::make_unique<Recorder>(*transport);

            programmer = std::make_unique<T>(CommandChannel(std::move(owned)), report);
        }

        NiceTransport *transport = nullptr;
        std::unique_ptr<Recorder> recorder;
        std::unique_ptr<T> programmer;
    };
} // namespace stlink::testing_support

#endif // STLINK_TESTS_PROGRAMMER_FIXTURE_H
