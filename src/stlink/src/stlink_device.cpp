/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-09-17.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */



#include <stlink/stlink_device.h>

namespace stlink {
    stlink_device::~stlink_device() = default;

// Buffers (read-only)
const unsigned char* stlink_device::c_buf() const noexcept { return c_buf_.data(); }
const unsigned char* stlink_device::q_buf() const noexcept { return q_buf_.data(); }
int32_t stlink_device::q_len() const noexcept { return q_len_; }

// General configuration/state (read-only)
int32_t stlink_device::verbose() const noexcept { return verbose_; }
int32_t stlink_device::opt() const noexcept { return opt_; }
uint32_t stlink_device::core_id() const noexcept { return core_id_; }
uint32_t stlink_device::chip_id() const noexcept { return chip_id_; }
target_state stlink_device::core_stat() const noexcept { return core_stat_; }

std::string_view stlink_device::serial() const noexcept { return std::string_view{serial_}; }
int32_t stlink_device::freq() const noexcept { return freq_; }

// Flash and memory characteristics (read-only)
stm32_flash_type stlink_device::flash_type() const noexcept { return flash_type_; }
stm32_addr_t stlink_device::flash_base() const noexcept { return flash_base_; }
uint32_t stlink_device::flash_size() const noexcept { return flash_size_; }
uint32_t stlink_device::flash_pgsz() const noexcept { return flash_pgsz_; }

// SRAM
stm32_addr_t stlink_device::sram_base() const noexcept { return sram_base_; }
uint32_t stlink_device::sram_size() const noexcept { return sram_size_; }

// Option bytes
stm32_addr_t stlink_device::option_base() const noexcept { return option_base_; }
uint32_t stlink_device::option_size() const noexcept { return option_size_; }

// System/bootloader region
stm32_addr_t stlink_device::sys_base() const noexcept { return sys_base_; }
uint32_t stlink_device::sys_size() const noexcept { return sys_size_; }

// Version and flags
const stlink_version_t& stlink_device::version() const noexcept { return version_; }
uint32_t stlink_device::chip_flags() const noexcept { return chip_flags_; }

// Trace & OTP
uint32_t stlink_device::max_trace_freq() const noexcept { return max_trace_freq_; }
uint32_t stlink_device::otp_base() const noexcept { return otp_base_; }
uint32_t stlink_device::otp_size() const noexcept { return otp_size_; }

} // stlink