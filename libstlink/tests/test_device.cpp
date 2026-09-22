/**
 * @file    test_device.cpp
 * @brief   The public device surface, as far as it goes without hardware.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Discovery and IDevice::create both reach USB through free functions, so
 * there is no seam to put a fake behind and they are not tested here. What is
 * testable is the vocabulary a caller sees and the default filter, and the
 * hardware smoke harness covers the rest.
 */

#include <stlink/device.h>

#include <gtest/gtest.h>

using namespace stlink;

TEST(Device, EveryProgrammerKindHasAName)
{
    EXPECT_STREQ(to_string(ProgrammerKind::V1), "ST-LINK/V1");
    EXPECT_STREQ(to_string(ProgrammerKind::V2), "ST-LINK/V2");
    EXPECT_STREQ(to_string(ProgrammerKind::V2_1), "ST-LINK/V2-1");
    EXPECT_STREQ(to_string(ProgrammerKind::V3), "STLINK-V3");
}

TEST(Device, AnUnknownKindSaysSoRatherThanReturningNothing)
{
    /*
     * Unlike to_string(FlashFamily), which returns nullptr for Unknown. This
     * one is printed straight into user-facing text, so it always has to be
     * something a caller can pass to a format string.
     */
    EXPECT_STREQ(to_string(ProgrammerKind::Unknown), "unknown");
}

TEST(Device, TheDefaultFilterAsksForNothing)
{
    EXPECT_FALSE(DeviceFilter::NoFilter.serial.has_value());
    EXPECT_FALSE(DeviceFilter::NoFilter.kind.has_value());
    EXPECT_FALSE(DeviceFilter::NoFilter.chip_id.has_value());
}

TEST(Device, AFreshFilterMatchesTheSharedDefault)
{
    const DeviceFilter fresh;

    EXPECT_EQ(fresh.serial.has_value(), DeviceFilter::NoFilter.serial.has_value());
    EXPECT_EQ(fresh.kind.has_value(), DeviceFilter::NoFilter.kind.has_value());
    EXPECT_EQ(fresh.chip_id.has_value(), DeviceFilter::NoFilter.chip_id.has_value());
}
