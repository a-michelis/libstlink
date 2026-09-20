/**
 * @file    test_chip_database.cpp
 * @brief   Holding the chip descriptions, and finding one.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ChipDatabase::instance() is process wide, so these tests use their own
 * instances where they can and touch the shared one only to prove it is there.
 */

#include <stlink/chip_database.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

using namespace stlink;

namespace
{
    ChipDescription made(std::uint32_t id, const char *name, FlashFamily family)
    {
        ChipDescription chip;
        chip.chip_id = id;
        chip.name = name;
        chip.family = family;

        return chip;
    }

    /** @brief A directory of .chip files that cleans up after itself. */
    class ChipsDirectory
    {
    public:
        ChipsDirectory()
            : path_(std::filesystem::temp_directory_path() /
                    ("stlink_test_chips_" + std::to_string(
                         std::chrono::steady_clock::now().time_since_epoch().count())))
        {
            std::filesystem::create_directories(path_);
        }

        ~ChipsDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }

        void write(const char *filename, const std::string &contents) const
        {
            std::ofstream file(path_ / filename);
            file << contents;
        }

        [[nodiscard]] std::string string() const
        {
            return path_.string();
        }

    private:
        std::filesystem::path path_;
    };
} // namespace

TEST(ChipDatabase, FindsWhatWasAdded)
{
    ChipDatabase database;
    database.add(made(0x410, "F1_MD", FlashFamily::F0_F1_F3));

    const ChipDescription *found = database.find(0x410);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "F1_MD");
    EXPECT_EQ(database.size(), 1u);
}

TEST(ChipDatabase, DoesNotInventWhatIsAbsent)
{
    ChipDatabase database;
    database.add(made(0x410, "F1_MD", FlashFamily::F0_F1_F3));

    EXPECT_EQ(database.find(0x999), nullptr);
}

TEST(ChipDatabase, LaterAdditionReplacesAnEarlierOne)
{
    ChipDatabase database;
    database.add(made(0x410, "first", FlashFamily::F0_F1_F3));
    database.add(made(0x410, "second", FlashFamily::F7));

    const ChipDescription *found = database.find(0x410);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "second");
    EXPECT_EQ(found->family, FlashFamily::F7);

    /* Replaced, not appended. */
    EXPECT_EQ(database.size(), 1u);
}

TEST(ChipDatabase, ReadsADirectoryOfDescriptions)
{
    const ChipsDirectory chips;
    chips.write("a.chip", "name A\nchip_id 0x100\nfamily F7\n");
    chips.write("b.chip", "name B\nchip_id 0x200\nfamily G0\n");

    ChipDatabase database;

    EXPECT_EQ(database.add_directory(chips.string()), 2u);
    ASSERT_NE(database.find(0x100), nullptr);
    ASSERT_NE(database.find(0x200), nullptr);
    EXPECT_EQ(database.find(0x100)->name, "A");
}

TEST(ChipDatabase, SkipsWhatIsNotAChipFile)
{
    const ChipsDirectory chips;
    chips.write("good.chip", "name A\nchip_id 0x100\nfamily F7\n");
    chips.write("notes.txt", "this is not a chip description at all\n");

    ChipDatabase database;

    EXPECT_EQ(database.add_directory(chips.string()), 1u);
}

TEST(ChipDatabase, OneBadFileDoesNotStopTheRest)
{
    const ChipsDirectory chips;
    chips.write("good.chip", "name A\nchip_id 0x100\nfamily F7\n");
    chips.write("bad.chip", "name B\nchip_id 0x200\nfamily NOPE\n");
    chips.write("also_good.chip", "name C\nchip_id 0x300\nfamily G0\n");

    ChipDatabase database;

    EXPECT_EQ(database.add_directory(chips.string()), 2u);
    EXPECT_NE(database.find(0x100), nullptr);
    EXPECT_EQ(database.find(0x200), nullptr);
    EXPECT_NE(database.find(0x300), nullptr);
}

TEST(ChipDatabase, AMissingDirectoryIsNotAnError)
{
    ChipDatabase database;

    EXPECT_EQ(database.add_directory("/there/is/nothing/here"), 0u);
    EXPECT_EQ(database.size(), 0u);
}

TEST(ChipDatabase, AFileOverridesOneAlreadyThere)
{
    const ChipsDirectory chips;
    chips.write("override.chip", "name replacement\nchip_id 0x410\nfamily F7\n");

    ChipDatabase database;
    database.add(made(0x410, "original", FlashFamily::F0_F1_F3));
    database.add_directory(chips.string());

    ASSERT_NE(database.find(0x410), nullptr);
    EXPECT_EQ(database.find(0x410)->name, "replacement");
    EXPECT_EQ(database.size(), 1u);
}

TEST(ChipDatabase, TheSharedInstanceIsOne)
{
    EXPECT_EQ(&ChipDatabase::instance(), &ChipDatabase::instance());
}
