/**
 * @file    programmer_mock.h
 * @brief   A mock programmer, and a fake target behind it.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The counterpart to programmer_fixture.h. That one fakes a transport, for
 * testing the programmers themselves; this one fakes a programmer, for
 * testing everything above them.
 */

#ifndef STLINK_TESTS_PROGRAMMER_MOCK_H
#define STLINK_TESTS_PROGRAMMER_MOCK_H

#include <stlink/programmer.h>

#include <cstdint>
#include <map>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stlink/cortex.h>

namespace stlink::testing_support
{
    class MockProgrammer : public IProgrammer
    {
    public:
        MOCK_METHOD(const ProgrammerVersion &, version, (), (const, noexcept, override));
        MOCK_METHOD(const ProbeAddress &, address, (), (const, noexcept, override));
        MOCK_METHOD(Result<ProgrammerMode>, mode, (), (override));
        MOCK_METHOD(Result<std::uint32_t>, target_voltage, (), (override));
        MOCK_METHOD(Result<std::vector<std::uint32_t>>, clock_rates, (), (override));
        MOCK_METHOD(VoidResult, set_clock, (std::uint32_t hz), (override));
        MOCK_METHOD(std::uint32_t, max_trace_frequency, (), (const, noexcept, override));

        MOCK_METHOD(VoidResult, exit_dfu, (), (override));
        MOCK_METHOD(VoidResult, enter_debug, (DebugMode debug), (override));
        MOCK_METHOD(VoidResult, exit_debug, (), (override));
        MOCK_METHOD(VoidResult, select_access_port, (std::uint8_t ap), (override));
        MOCK_METHOD(Result<std::uint32_t>, core_id, (), (override));

        MOCK_METHOD(Result<CoreState>, state, (), (override));
        MOCK_METHOD(VoidResult, halt, (), (override));
        MOCK_METHOD(VoidResult, run, (bool flash_loader), (override));
        MOCK_METHOD(VoidResult, step, (), (override));
        MOCK_METHOD(VoidResult, reset, (), (override));
        MOCK_METHOD(VoidResult, reset_pin, (bool asserted), (override));

        MOCK_METHOD(VoidResult, read_memory,
                    (std::uint32_t address, std::uint8_t *data, std::size_t size), (override));
        MOCK_METHOD(VoidResult, write_memory,
                    (std::uint32_t address, const std::uint8_t *data, std::size_t size),
                    (override));

        MOCK_METHOD(Result<std::uint32_t>, read_debug_register, (std::uint32_t address),
                    (override));
        MOCK_METHOD(VoidResult, write_debug_register, (std::uint32_t address, std::uint32_t value),
                    (override));

        MOCK_METHOD(Result<std::uint32_t>, read_register, (std::uint8_t index), (override));
        MOCK_METHOD(VoidResult, write_register, (std::uint8_t index, std::uint32_t value),
                    (override));
        MOCK_METHOD(Result<CoreRegisters>, read_registers, (), (override));

        MOCK_METHOD(VoidResult, trace_enable, (std::uint32_t hz), (override));
        MOCK_METHOD(VoidResult, trace_disable, (), (override));
        MOCK_METHOD(Result<std::size_t>, trace_read, (std::uint8_t * data, std::size_t size),
                    (override));
    };

    using NiceProgrammer = ::testing::NiceMock<MockProgrammer>;

    /**
     * @brief A target that answers reads from a table and remembers the order.
     *
     * An address nobody set answers zero, which is exactly what the parts
     * without a DBGMCU_IDCODE do, so the fallback paths need no special
     * arrangement to exercise.
     */
    class FakeTarget
    {
    public:
        explicit FakeTarget(NiceProgrammer &programmer)
        {
            ON_CALL(programmer, core_id()).WillByDefault([this] {
                return Result<std::uint32_t>(core_id_);
            });

            ON_CALL(programmer, read_debug_register(::testing::_))
                .WillByDefault([this](std::uint32_t address) -> Result<std::uint32_t> {
                    read.push_back(address);

                    const auto found = values_.find(address);

                    return found == values_.end() ? 0u : found->second;
                });
        }

        /** @brief Make @p address answer @p value. */
        FakeTarget &at(std::uint32_t address, std::uint32_t value)
        {
            values_[address] = value;

            return *this;
        }

        /** @brief What the debug port identifies itself as. */
        FakeTarget &port(std::uint32_t core_id)
        {
            core_id_ = core_id;

            return *this;
        }

        /** @brief Set CPUID to describe a core of this part number. */
        FakeTarget &core(std::uint16_t part)
        {
            return at(kCpuid, (static_cast<std::uint32_t>(kImplementerArm) << 24) |
                                  (static_cast<std::uint32_t>(part) << 4));
        }

        /** @brief Every address read, in the order it was read. */
        std::vector<std::uint32_t> read;

    private:
        std::map<std::uint32_t, std::uint32_t> values_;
        std::uint32_t core_id_ = 0x2ba01477;
    };
} // namespace stlink::testing_support

#endif // STLINK_TESTS_PROGRAMMER_MOCK_H
