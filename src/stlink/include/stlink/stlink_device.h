/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-09-17.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */


#ifndef STLINK_STLINK_DEVICE_H
#define STLINK_STLINK_DEVICE_H

#include <string_view>
#include <tuple>
#include <stlink/common.h>
#include <array>

namespace stlink {
    /**
     * Represents an abstraction of an STLink device and its operations.
     * Provides access to the state, memory, and configurations of the device.
     */
    class stlink_device {
        /**
         * Destructor for the stlink_device class.
         *
         * Cleans up resources used by an instance of the stlink_device class.
         * This is a virtual destructor that ensures proper resource deallocation
         * in derived classes when a stlink_device object is deleted through
         * a base class pointer.
         */
    public:
        virtual ~stlink_device();

        /**
         * Provides read-only access to the constant buffer, which is an internal data
         * structure used by the stlink_device.
         *
         * @return A pointer to the internal constant buffer of type unsigned char.
         */
        [[nodiscard]] const unsigned char *c_buf() const noexcept;

        /**
         * Provides a pointer to the q_buf_ buffer, which is a read-only data buffer
         * used internally by the stlink_device class.
         *
         * @return A constant pointer to the q_buf_ buffer.
         */
        [[nodiscard]] const unsigned char *q_buf() const noexcept;

        /**
         * Retrieves the length of the internal buffer associated with the device.
         *
         * @return The length of the device's internal buffer as a 32-bit integer.
         */
        [[nodiscard]] int32_t q_len() const noexcept;

        /**
         * Retrieves the verbosity level of the device.
         *
         * @return The verbosity level as an integer value.
         */
        [[nodiscard]] int32_t verbose() const noexcept;

        /**
         * Retrieves the current option configuration value of the device.
         *
         * @return The option configuration value as an integer.
         */
        [[nodiscard]] int32_t opt() const noexcept;

        /**
         * Retrieves the core ID of the device.
         *
         * @return The core ID as a 32-bit unsigned integer.
         */
        [[nodiscard]] uint32_t core_id() const noexcept;

        /**
         * Retrieves the chip ID of the device.
         *
         * @return The 32-bit identifier representing the chip ID.
         */
        [[nodiscard]] uint32_t chip_id() const noexcept;

        /**
         * Retrieves the current state of the target device's core.
         *
         * @return The state of the target core, represented as an enum of type target_state.
         *         Possible values include TARGET_UNKNOWN, TARGET_RUNNING, TARGET_HALTED,
         *         TARGET_RESET, and TARGET_DEBUG_RUNNING.
         */
        [[nodiscard]] target_state core_stat() const noexcept;

        /**
         * Retrieves the serial number of the stlink_device.
         *
         * @return A read-only view (`std::string_view`) of the serial number buffer.
         */
        [[nodiscard]] std::string_view serial() const noexcept;

        /**
         * Retrieves the current communication frequency setting of the device.
         *
         * @return The communication frequency as an integer in Hz.
         */
        [[nodiscard]] int32_t freq() const noexcept;

        /**
         * Retrieves the flash type associated with the device.
         *
         * @return The flash type as a value of the `stm32_flash_type` enumeration.
         *         This indicates the specific flash memory type supported by the device.
         */
        [[nodiscard]] stm32_flash_type flash_type() const noexcept;

        /**
         * Retrieves the base address of the flash memory.
         *
         * @return The base address of the flash memory as a value of type stm32_addr_t.
         */
        [[nodiscard]] stm32_addr_t flash_base() const noexcept;

        /**
         * Retrieves the flash size of the device.
         *
         * @return The size of the flash memory in bytes.
         */
        [[nodiscard]] uint32_t flash_size() const noexcept;

        /**
         * Retrieves the flash page size of the device.
         *
         * @return The size of a single flash memory page in bytes.
         */
        [[nodiscard]] uint32_t flash_pgsz() const noexcept;

        /**
         * Retrieves the base address of the SRAM memory region.
         *
         * @return The starting address of the SRAM region as a 32-bit unsigned integer (stm32_addr_t).
         */
        [[nodiscard]] stm32_addr_t sram_base() const noexcept;

        /**
         * Retrieves the size of the SRAM memory for the current device.
         *
         * @return The size of the SRAM in bytes.
         */
        [[nodiscard]] uint32_t sram_size() const noexcept;

        /**
         * Retrieves the base address of the option bytes memory region of the device.
         *
         * @return The base address of the option bytes as a `stm32_addr_t`.
         */
        [[nodiscard]] stm32_addr_t option_base() const noexcept;

        /**
         * Retrieves the size of the option bytes region for the device.
         *
         * @return The size of the option bytes region as a 32-bit unsigned integer.
         */
        [[nodiscard]] uint32_t option_size() const noexcept;

        /**
         * Retrieves the base address of the system/bootloader region.
         *
         * @return The base address of the system/bootloader region as a `stm32_addr_t`.
         */
        [[nodiscard]] stm32_addr_t sys_base() const noexcept;

        /**
         * Retrieves the size of the system memory for the device.
         *
         * @return The size of the system memory in bytes.
         */
        [[nodiscard]] uint32_t sys_size() const noexcept;

        /**
         * @brief Retrieves the version information of the ST-Link device.
         *
         * This method provides information about the hardware version,
         * protocol versions, vendor ID, product ID, supported API versions,
         * and feature flags of the ST-Link device.
         *
         * @return A constant reference to an stlink_version_t struct containing
         *         the version details of the device.
         */
        [[nodiscard]] const stlink_version_t &version() const noexcept;

        /**
         * Retrieves the chip-specific flags associated with the device.
         *
         * @return A 32-bit unsigned integer representing the chip flags.
         */
        [[nodiscard]] uint32_t chip_flags() const noexcept;

        /**
         * Retrieves the maximum supported trace frequency of the device.
         *
         * @return The maximum trace frequency in hertz as a 32-bit unsigned integer.
         */
        [[nodiscard]] uint32_t max_trace_freq() const noexcept;

        /**
         * Retrieves the base address of the One-Time Programmable (OTP) memory region.
         *
         * @return The base address of the OTP memory region as a 32-bit unsigned integer.
         */
        [[nodiscard]] uint32_t otp_base() const noexcept;

        /**
         * Retrieves the size of the one-time programmable (OTP) memory region.
         *
         * @return The size of the OTP memory in bytes.
         */
        [[nodiscard]] uint32_t otp_size() const noexcept;

    public:
        // Modern C++17 interface (pure virtual). Uses Status/StatusOr and spans from common.h.
        // Implementations should keep mirrored fields in sync with accessors.

        // Modes / status / version / frequency / voltage
        virtual stlink::Status enter_swd_mode() = 0;
        virtual stlink::Status exit_debug_mode() = 0;
        virtual stlink::Status exit_dfu_mode() = 0;

        virtual stlink::StatusOr<stlink::DeviceMode> current_mode() = 0;
        virtual stlink::Status status_refresh() = 0;           // updates core_stat()
        virtual stlink::Status read_version_refresh() = 0;     // updates version(), max_trace_freq()

        // Set SWD clock in kHz; return actual set value if backend quantizes.
        virtual stlink::StatusOr<int32_t> set_swdclk_khz(int32_t freq_khz) = 0;

        // Target voltage in millivolts (implementation-defined if not available)
        virtual stlink::StatusOr<int32_t> target_voltage_mv() = 0;

        virtual stlink::Status target_connect(stlink::ConnectType how) = 0;

        // Core control / identification
        virtual stlink::Status core_id_read_refresh() = 0;     // updates core_id()
        virtual stlink::StatusOr<stlink::Cpuid> cpu_id() = 0;

        virtual stlink::Status reset(stlink::ResetType type) = 0;
        virtual stlink::Status run(stlink::RunType how) = 0;
        virtual stlink::Status run_at(stm32_addr_t addr) = 0;
        virtual stlink::Status step() = 0;
        virtual stlink::Status force_debug() = 0;
        virtual stlink::StatusOr<bool> is_core_halted() = 0;

        // Memory and register access
        // Prefer spans; keep alignment constraints identical to the C API.
        virtual stlink::StatusOr<uint32_t> read_debug32(uint32_t addr) = 0;
        virtual stlink::Status write_debug32(uint32_t addr, uint32_t value) = 0;

        // Read len bytes (len % 4 == 0) into caller-provided buffer.
        virtual stlink::Status read_mem32(uint32_t addr, stlink::MutableByteSpan out) = 0;

        // Write len bytes (len % 4 == 0) from caller buffer to addr.
        virtual stlink::Status write_mem32(uint32_t addr, stlink::ByteSpan data) = 0;

        // Write arbitrary-sized buffer to addr (byte access).
        virtual stlink::Status write_mem8(uint32_t addr, stlink::ByteSpan data) = 0;

        // Registers
        virtual stlink::StatusOr<stlink::Reg> read_reg(int32_t index) = 0;
        virtual stlink::Status write_reg(uint32_t value, int32_t index) = 0;
        virtual stlink::StatusOr<stlink::Reg> read_all_regs() = 0;

        virtual stlink::StatusOr<stlink::Reg> read_unsupported_reg(int32_t index) = 0;
        virtual stlink::Status write_unsupported_reg(uint32_t value, int32_t index, stlink::Reg& inout) = 0;
        virtual stlink::StatusOr<stlink::Reg> read_all_unsupported_regs() = 0;

        // Flash operations
        virtual stlink::Status load_device_params_refresh() = 0; // updates flash/sram/sys/flags mirrors
        virtual uint32_t calculate_pagesize(uint32_t flashaddr) = 0;

        virtual stlink::Status check_address_range_validity(stm32_addr_t addr, uint32_t size) = 0;
        virtual stlink::Status check_address_range_validity_otp(stm32_addr_t addr, uint32_t size) = 0;
        virtual stlink::Status check_address_alignment(stm32_addr_t addr) = 0;

        virtual stlink::Status erase_flash_page(stm32_addr_t flashaddr) = 0;
        virtual stlink::Status erase_flash_section(stm32_addr_t base_addr, uint32_t size, bool align_size) = 0;
        virtual stlink::Status erase_flash_mass() = 0;

        // Program/verify
        virtual stlink::Status write_flash(stm32_addr_t addr, stlink::ByteSpan data,
                                           uint8_t erase_only, stlink::EraseType erase) = 0;
        virtual stlink::Status write_otp(stm32_addr_t addr, stlink::ByteSpan data) = 0;

        virtual stlink::Status mwrite_flash(stlink::ByteSpan data, uint32_t length,
                                            stm32_addr_t addr, stlink::EraseType erase) = 0;
        virtual stlink::Status fwrite_flash(const char* path, stm32_addr_t addr, stlink::EraseType erase) = 0;
        virtual stlink::Status fcheck_flash(const char* path, stm32_addr_t addr) = 0;
        virtual stlink::Status verify_write_flash(stm32_addr_t addr, stlink::ByteSpan data) = 0;
        virtual void          fwrite_finalize(stm32_addr_t where) = 0;

        // Option bytes (generic 32-bit variants; series-specific can be added similarly)
        virtual stlink::StatusOr<uint32_t> read_option_control_register32() = 0;
        virtual stlink::Status write_option_control_register32(uint32_t option_cr) = 0;
        virtual stlink::StatusOr<uint32_t> read_option_control_register1_32() = 0;
        virtual stlink::Status write_option_control_register1_32(uint32_t option_cr1) = 0;
        virtual stlink::StatusOr<uint32_t> read_option_bytes32() = 0;
        virtual stlink::Status write_option_bytes32(uint32_t option_byte) = 0;
        virtual stlink::StatusOr<uint32_t> read_option_bytes_boot_add32() = 0;
        virtual stlink::Status write_option_bytes_boot_add32(uint32_t option_bytes_boot_add) = 0;

        // File helpers / SRAM
        virtual stlink::StatusOr<std::tuple<uint8_t*, uint32_t, uint32_t>>
            parse_ihex(const char* path, uint8_t erased_pattern) = 0; // returns (mem,size,begin)
        virtual uint8_t get_erased_pattern() = 0;
        virtual stlink::Status mwrite_sram(stlink::ByteSpan data, uint32_t length, stm32_addr_t addr) = 0;
        virtual stlink::Status fwrite_sram(const char* path, stm32_addr_t addr) = 0;
        virtual stlink::Status fread_to_file(const char* path, bool is_ihex, stm32_addr_t addr, uint32_t size) = 0;
        virtual void           print_data() = 0;

    private:
        // Mirrored fields from struct _stlink (kept private)

        /**
         * @brief Internal buffer used for communication or data processing within the stlink_device class.
         *
         * This buffer holds a fixed-length array of unsigned char elements, defined by the constant C_BUF_LEN.
         * It is primarily used for internal operations such as data handling, command generation,
         * or response processing, serving as a temporary storage mechanism.
         *
         * The buffer size is pre-determined and can be referenced via the macro `C_BUF_LEN`.
         */
        std::array<unsigned char, C_BUF_LEN> c_buf_ = {};
        /**
         * @brief Internal buffer used in the `stlink_device` class for storing
         *        auxiliary data.
         *
         * This buffer has a fixed size defined by the macro `Q_BUF_LEN` (set to 1024 * 100),
         * allowing it to hold up to 100 KB of data. It is used within the class for
         * operations requiring temporary storage and is accessible via the `q_buf()` method.
         *
         * @note Primarily intended for internal use within the `stlink_device` implementation.
         */
        std::array<unsigned char, Q_BUF_LEN> q_buf_ = {};
        /**
         * Stores the length of the query buffer.
         *
         * This variable holds the size or length of the data stored in the query
         * buffer (`q_buf_`). It is primarily used internally within the class to
         * manage and track the buffer's content size.
         *
         * The value is returned by the `q_len()` method of the `stlink_device` class.
         * It is initialized to zero.
         */
        int32_t q_len_ = 0;

        /**
         * Stores the verbosity level of the device. This value controls the level
         * of debug or log information generated during the operation of the
         * stlink_device. Higher values may correspond to more detailed output.
         *
         * Default value is 0, indicating minimal or no additional debug information.
         */
        int32_t verbose_ = 0;
        /**
         * @brief Represents optional configuration settings or flags used internally by the stlink_device class.
         *
         * This variable stores an integer value that may serve as customization or parameterization options
         * for the operations executed by the device class. It is initialized to 0 by default.
         */
        int32_t opt_ = 0;
        /**
         * Stores the Core ID of the connected device.
         *
         * This variable is used internally to represent the unique identifier of the core
         * for the connected device. It is typically set and accessed through the
         * `stlink_device::core_id()` method.
         *
         * Default value: `0`
         */
        uint32_t core_id_ = 0;
        /**
         * @brief Stores the unique identifier of the chip.
         *
         * This variable holds the chip ID, which is used to identify the specific
         * chip type or family. It is typically retrieved from the connected hardware
         * during initialization or communication processes.
         *
         * @note This value is utilized in multiple operations that require chip-specific
         * details, such as configuration or debugging.
         */
        uint32_t chip_id_ = 0;
        /**
         * @brief Represents the current state of the target device's core.
         *
         * This variable holds the target's execution state, which is one of the values
         * defined in the `target_state` enumeration. Possible states include:
         * - `TARGET_UNKNOWN`: The target state is unknown or cannot be determined.
         * - `TARGET_RUNNING`: The target is running normally.
         * - `TARGET_HALTED`: The target is halted (stopped).
         * - `TARGET_RESET`: The target is in a reset state.
         * - `TARGET_DEBUG_RUNNING`: The target is running in debug mode.
         *
         * The default value is `TARGET_UNKNOWN`.
         *
         * This variable is used internally by the `stlink_device` class to mirror
         * the core execution state of the device.
         */
        enum target_state core_stat_ = TARGET_UNKNOWN;

        /**
         * @brief Buffer to store the serial number of the ST-Link device.
         *
         * This buffer is used to hold the serial number string of the ST-Link device.
         * Its size is defined by the constant `STLINK_SERIAL_BUFFER_SIZE`, which is
         * one byte larger than the length of the serial number (`STLINK_SERIAL_LENGTH`)
         * to accommodate the null-terminator for string termination.
         */
        char serial_[STLINK_SERIAL_BUFFER_SIZE] = {};
        /**
         * Represents the frequency parameter associated with the device.
         * Typically defines the operational clock frequency or communication
         * speed used within the context of an stlink device.
         *
         * Default value is initialized to `0`.
         */
        int32_t freq_ = 0;

        /**
         * @brief Represents the type of flash memory associated with the device.
         *
         * This variable is used to specify the type of flash memory utilized by the STM32 device.
         * The value is initialized to `STM32_FLASH_TYPE_UNKNOWN`, indicating an unknown or unsupported
         * flash type until it is explicitly determined during the device initialization or operation.
         *
         * Flash types are categorized based on the STM32 series, such as STM32C0, STM32F1, etc.,
         * and are defined in the `stm32_flash_type` enumeration.
         */
        enum stm32_flash_type flash_type_ = STM32_FLASH_TYPE_UNKNOWN;
        /**
         * @brief Base address of the flash memory.
         *
         * This variable holds the starting address of the flash memory for the
         * connected STM32 device. It is represented as an address type (`stm32_addr_t`),
         * which is typically a 32-bit unsigned integer corresponding to the memory
         * layout of the STM32 microcontroller.
         *
         * The value is initialized to 0 and may be updated based on the specific
         * device being used.
         */
        stm32_addr_t flash_base_ = 0;
        /**
         * @brief Represents the size of the flash memory in bytes.
         *
         * This variable stores the total size of the flash memory
         * available on the device. It is used internally to manage
         * and query flash-related operations.
         */
        uint32_t flash_size_ = 0;
        /**
         * Represents the memory page size of the flash on the device.
         *
         * This variable stores the size of a single flash memory page in bytes
         * for the associated target device. It is used to determine the page
         * boundaries during flash programming or erasure operations.
         * Its value is initialized to zero and should be updated with the
         * correct page size after identifying the target device.
         */
        uint32_t flash_pgsz_ = 0;

        /**
         * @brief Represents the base address of the SRAM for the device.
         *
         * This variable holds the starting memory address of the SRAM region.
         * It is used internally by the `stlink_device` class to interact with
         * the SRAM of the target device. The value of `sram_base_` is typically
         * determined based on the target microcontroller and its memory layout.
         */
        stm32_addr_t sram_base_ = 0;
        /**
         * Represents the size of the SRAM memory associated with the device, in bytes.
         * This variable is used internally to store the determined or configured SRAM size.
         * It plays a key role in the memory management of the device.
         */
        uint32_t sram_size_ = 0;

        /**
         * @brief Represents the base address of the option bytes in STM32 memory.
         *
         * This variable is used to store the starting memory address where
         * the option bytes are located in the STM32 device. The value
         * is defined as a 32-bit address (stm32_addr_t) and can be used
         * in memory operations related to the option byte area.
         *
         * It plays a crucial role in interacting with STM32 devices,
         * particularly for configuring or reading device-specific options
         * stored in the option byte region.
         */
        stm32_addr_t option_base_ = 0;
        /**
         * Represents the size of the option bytes region for a particular device.
         * Used to determine the memory space allocated for configuration options.
         */
        uint32_t option_size_ = 0;

        /**
         * @brief Represents the base address of the system memory region.
         *
         * This variable stores the starting memory address for the system memory region
         * on an STM32 microcontroller. It is defined as a 32-bit unsigned integer type,
         * `stm32_addr_t`. Typically used in low-level device interaction for accessing
         * system memory.
         */
        stm32_addr_t sys_base_ = 0;
        /**
         * Stores the size of the system memory in bytes.
         *
         * This variable holds the size of the system memory for the connected device.
         * It is used internally by the `stlink_device` class to represent system
         * memory information and is returned by the `sys_size()` accessor method.
         */
        uint32_t sys_size_ = 0;

        /**
         * @brief Represents the version information of a connected ST-Link device.
         *
         * The version_ variable is used to store version details for the ST-Link device,
         * including hardware version, protocol versions (JTAG/SWIM), vendor ID, product ID,
         * API version, and supported feature flags.
         *
         * This variable plays a key role in identifying the capabilities of the device
         * and ensuring compatibility with certain operations.
         */
        stlink_version_t version_ = {};
        /**
         * @brief Stores the flags representing specific characteristics or
         * configuration details of the chip being accessed.
         *
         * This variable is used internally to maintain state or metadata
         * associated with the chip, which may affect operations or behavior
         * during interaction with the device.
         */
        uint32_t chip_flags_ = 0;

        /**
         * @brief Represents the maximum trace frequency supported by the device.
         *
         * This variable stores the upper limit for the trace clock frequency that
         * the device can handle. It is used to configure and manage trace operations.
         *
         * @details The initial value is set to 0, indicating that no specific frequency
         * has been assigned. The value can be updated as needed based on the device's
         * capabilities and requirements.
         */
        uint32_t max_trace_freq_ = 0;

        /**
         * @brief Represents the base address of the One-Time Programmable (OTP) memory region.
         *
         * The otp_base_ variable holds the starting memory address where the OTP memory
         * is located for a device. This value is typically set internally based on the
         * specific device configuration or derived from device-specific information.
         */
        uint32_t otp_base_ = 0;
        /**
         * @brief Represents the size of the One-Time Programmable (OTP) memory.
         *
         * This variable holds the size of the OTP memory in bytes for the connected
         * device. It is used internally by the stlink_device class to manage and
         * retrieve OTP memory-related information.
         *
         * @note This value is initialized to 0 by default and is updated during
         *       device initialization or when queried using the appropriate methods.
         */
        uint32_t otp_size_ = 0;
    };
} // stlink

#endif //STLINK_STLINK_DEVICE_H
