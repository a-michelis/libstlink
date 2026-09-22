/**
 * @file    test_chip_description.cpp
 * @brief   Reading a chip description file.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/chip_description.h>

#include <gtest/gtest.h>

using namespace stlink;

namespace
{
    constexpr const char *kMinimal = "name X\nchip_id 0x410\nfamily F0_F1_F3\n";
}

TEST(ChipDescription, ReadsEveryField)
{
    auto parsed = parse_chip_description(
        "# a comment\n"
        "name             STM32F1xx_MD\n"
        "reference        RM0008\n"
        "chip_id          0x410       # trailing comment\n"
        "family           F0_F1_F3\n"
        "\n"
        "flash.base       0x08000000\n"
        "flash.size_reg   0x1ffff7e0\n"
        "flash.page_size  0x400\n"
        "\n"
        "sram.base        0x20000000\n"
        "sram.size        0x5000\n"
        "\n"
        "bootrom.base     0x1ffff000\n"
        "bootrom.size     0x800\n"
        "\n"
        "option.base      0x1ffff800\n"
        "option.size      0x10\n"
        "\n"
        "otp.base         0x1fff7000\n"
        "otp.size         0x400\n"
        "\n"
        "flags            swo dualbank\n");

    ASSERT_TRUE(parsed.ok()) << parsed.error().describe();

    const ChipDescription chip = parsed.value();

    EXPECT_EQ(chip.name, "STM32F1xx_MD");
    EXPECT_EQ(chip.reference, "RM0008");
    EXPECT_EQ(chip.chip_id, 0x410u);
    EXPECT_EQ(chip.family, FlashFamily::F0_F1_F3);

    EXPECT_EQ(chip.flash.base, 0x08000000u);
    EXPECT_EQ(chip.flash_size_reg, 0x1ffff7e0u);
    EXPECT_EQ(chip.flash_page_size, 0x400u);

    /* The size is read from the chip, never from the file. */
    EXPECT_EQ(chip.flash.size, 0u);

    EXPECT_EQ(chip.sram.base, 0x20000000u);
    EXPECT_EQ(chip.sram.size, 0x5000u);
    EXPECT_EQ(chip.bootrom.base, 0x1ffff000u);
    EXPECT_EQ(chip.bootrom.size, 0x800u);
    EXPECT_EQ(chip.option_bytes.base, 0x1ffff800u);
    EXPECT_EQ(chip.option_bytes.size, 0x10u);
    EXPECT_EQ(chip.otp.base, 0x1fff7000u);
    EXPECT_EQ(chip.otp.size, 0x400u);

    EXPECT_TRUE(has_flag(chip.flags, ChipFlags::Swo));
    EXPECT_TRUE(has_flag(chip.flags, ChipFlags::DualBank));
}

TEST(ChipDescription, DefaultsWhatIsAbsent)
{
    auto parsed = parse_chip_description(kMinimal);

    ASSERT_TRUE(parsed.ok()) << parsed.error().describe();

    const ChipDescription chip = parsed.value();

    EXPECT_EQ(chip.otp.size, 0u);
    EXPECT_EQ(chip.flags, ChipFlags::None);
    EXPECT_TRUE(chip.reference.empty());
}

TEST(ChipDescription, AcceptsDecimalAndHex)
{
    auto parsed = parse_chip_description("name X\nchip_id 1040\nfamily F7\nsram.size 0x100\n");

    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.value().chip_id, 1040u);
}

/* Whitespace and line endings a file might plausibly arrive with. */

TEST(ChipDescription, AcceptsCrLf)
{
    EXPECT_TRUE(parse_chip_description("name X\r\nchip_id 1\r\nfamily F7\r\n").ok());
}

TEST(ChipDescription, AcceptsNoTrailingNewline)
{
    EXPECT_TRUE(parse_chip_description("name X\nchip_id 1\nfamily F7").ok());
}

TEST(ChipDescription, AcceptsTabs)
{
    EXPECT_TRUE(parse_chip_description("name\tX\nchip_id\t1\nfamily\tF7\n").ok());
}

/*
 * Everything below is rejected on purpose. A typo in a description should be
 * reported rather than quietly leaving a field at zero.
 */

TEST(ChipDescription, RejectsAnUnknownField)
{
    auto parsed = parse_chip_description("name X\nchip_id 1\nfamily F7\nbogus 4\n");

    ASSERT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.error().code(), ErrorCode::InvalidArgument);
}

TEST(ChipDescription, RejectsAnUnknownFamily)
{
    EXPECT_FALSE(parse_chip_description("name X\nchip_id 1\nfamily Z9\n").ok());
}

TEST(ChipDescription, RejectsAnUnknownFlag)
{
    EXPECT_FALSE(parse_chip_description("name X\nchip_id 1\nfamily F7\nflags wibble\n").ok());
}

TEST(ChipDescription, RejectsANumberThatIsNotOne)
{
    EXPECT_FALSE(parse_chip_description("name X\nchip_id 0x4z0\nfamily F7\n").ok());
}

TEST(ChipDescription, RejectsTrailingJunkAfterANumber)
{
    EXPECT_FALSE(parse_chip_description("name X\nchip_id 12abc\nfamily F7\n").ok());
}

TEST(ChipDescription, RejectsAValueThatDoesNotFit)
{
    EXPECT_FALSE(parse_chip_description("name X\nchip_id 0x1FFFFFFFF\nfamily F7\n").ok());
}

TEST(ChipDescription, RejectsAFieldWithNoValue)
{
    EXPECT_FALSE(parse_chip_description("name X\nchip_id\nfamily F7\n").ok());
}

TEST(ChipDescription, RejectsAMissingName)
{
    EXPECT_FALSE(parse_chip_description("chip_id 1\nfamily F7\n").ok());
}

TEST(ChipDescription, RejectsAMissingChipId)
{
    EXPECT_FALSE(parse_chip_description("name X\nfamily F7\n").ok());
}

TEST(ChipDescription, RejectsAMissingFamily)
{
    EXPECT_FALSE(parse_chip_description("name X\nchip_id 1\n").ok());
}

TEST(ChipDescription, RejectsNothingAtAll)
{
    EXPECT_FALSE(parse_chip_description("").ok());
    EXPECT_FALSE(parse_chip_description("# just a comment\n\n   \n").ok());
}

TEST(ChipDescription, FamilyNamesRoundTrip)
{
    for (const char *name : {"C0", "C5", "F0_F1_F3", "F1_XL", "F2_F4", "F7", "G0", "G4", "H5",
                             "H7", "L0_L1", "L4", "L5_U5", "WB_WL", "WB0"})
    {
        const FlashFamily family = flash_family_from_string(name);

        ASSERT_NE(family, FlashFamily::Unknown) << name;
        EXPECT_STREQ(to_string(family), name);
    }
}

TEST(ChipDescription, UnknownFamilyHasNoName)
{
    EXPECT_EQ(flash_family_from_string("nonsense"), FlashFamily::Unknown);
    EXPECT_EQ(to_string(FlashFamily::Unknown), nullptr);
}

TEST(FlashSize, ReadsTheLowerHalfwordWhenTheRegisterIsWordAligned)
{
    ChipDescription chip;
    chip.flash_size_reg = 0x1ffff7e0;

    /* The upper half is something else entirely and must be ignored. */
    EXPECT_EQ(flash_size_from(chip, 0xdead0080), 128u * 1024u);
}

TEST(FlashSize, ReadsTheUpperHalfwordWhenTheRegisterEndsInTwo)
{
    ChipDescription chip;
    chip.flash_size_reg = 0x1fff7a22;

    /* An F411 keeps its size in the upper half of the word at ...20. */
    EXPECT_EQ(flash_size_from(chip, 0x0200beef), 512u * 1024u);
}

TEST(FlashSize, ReportsBytesRatherThanWhatTheChipSays)
{
    ChipDescription chip;
    chip.flash_size_reg = 0x1ffff7e0;

    EXPECT_EQ(flash_size_from(chip, 64), 64u * 1024u);
}

TEST(FlashSize, IsZeroWhenTheChipKeepsNoSizeRegister)
{
    ChipDescription chip;
    chip.flash_size_reg = 0;

    /* Nothing was read, so nothing is claimed, whatever the word holds. */
    EXPECT_EQ(flash_size_from(chip, 0xffffffff), 0u);
}
