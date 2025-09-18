/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-09-17.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */


#ifndef STLINK_COMMON_H
#define STLINK_COMMON_H
#include <cstdint>


/** @brief Core running status code */
#define STLINK_CORE_RUNNING             0x80
/** @brief Core halted status code */
#define STLINK_CORE_HALTED              0x81

/** @brief Device operation modes */
/** @{ */
#define STLINK_DEV_DFU_MODE             0x00    /**< Device Firmware Upgrade mode */
#define STLINK_DEV_MASS_MODE            0x01    /**< Mass Storage mode */
#define STLINK_DEV_DEBUG_MODE           0x02    /**< Debug mode */
#define STLINK_DEV_UNKNOWN_MODE           -1    /**< Unknown mode */
/** @} */

/** @brief NRST pin control values */
/** @{ */
#define STLINK_DEBUG_APIV2_DRIVE_NRST_LOW  0x00 /**< Drive NRST pin low */
#define STLINK_DEBUG_APIV2_DRIVE_NRST_HIGH 0x01 /**< Drive NRST pin high */
/** @} */

/** @brief SWDCLK frequency divisors */
/** @{ */
#define STLINK_SWDCLK_4MHZ_DIVISOR        0    /**< 4 MHz clock */
#define STLINK_SWDCLK_1P8MHZ_DIVISOR      1    /**< 1.8 MHz clock */
#define STLINK_SWDCLK_1P2MHZ_DIVISOR      2    /**< 1.2 MHz clock */
#define STLINK_SWDCLK_950KHZ_DIVISOR      3    /**< 950 KHz clock */
#define STLINK_SWDCLK_480KHZ_DIVISOR      7    /**< 480 KHz clock */
#define STLINK_SWDCLK_240KHZ_DIVISOR     15    /**< 240 KHz clock */
#define STLINK_SWDCLK_125KHZ_DIVISOR     31    /**< 125 KHz clock */
#define STLINK_SWDCLK_100KHZ_DIVISOR     40    /**< 100 KHz clock */
#define STLINK_SWDCLK_50KHZ_DIVISOR      79    /**< 50 KHz clock */
#define STLINK_SWDCLK_25KHZ_DIVISOR     158    /**< 25 KHz clock */
#define STLINK_SWDCLK_15KHZ_DIVISOR     265    /**< 15 KHz clock */
#define STLINK_SWDCLK_5KHZ_DIVISOR      798    /**< 5 KHz clock */
/** @} */

/** @brief Serial number related constants */
/** @{ */
#define STLINK_SERIAL_LENGTH             24     /**< Length of serial number */
#define STLINK_SERIAL_BUFFER_SIZE        (STLINK_SERIAL_LENGTH + 1) /**< Buffer size for serial number including null terminator */

/** @} */

/** @brief V3 specific constants */
#define STLINK_V3_MAX_FREQ_NB            10    /**< Maximum frequency number for V3 */

/** @brief Trace buffer and frequency constants */
/** @{ */
#define STLINK_V2_TRACE_BUF_LEN            2048   /**< V2 trace buffer length */
#define STLINK_V3_TRACE_BUF_LEN            8192   /**< V3 trace buffer length */
#define STLINK_V2_MAX_TRACE_FREQUENCY   2000000   /**< V2 maximum trace frequency */
#define STLINK_V3_MAX_TRACE_FREQUENCY  24000000   /**< V3 maximum trace frequency */
#define STLINK_DEFAULT_TRACE_FREQUENCY  2000000   /**< Default trace frequency */
/** @} */

/** @brief Feature flags for firmware capabilities */
/** @{ */
#define STLINK_F_HAS_TRACE              (1 << 0) /**< Has trace capability */
#define STLINK_F_HAS_SWD_SET_FREQ       (1 << 1) /**< Can set SWD frequency */
#define STLINK_F_HAS_JTAG_SET_FREQ      (1 << 2) /**< Can set JTAG frequency */
#define STLINK_F_HAS_MEM_16BIT          (1 << 3) /**< Supports 16-bit memory access */
#define STLINK_F_HAS_GETLASTRWSTATUS2   (1 << 4) /**< Has extended R/W status */
#define STLINK_F_HAS_DAP_REG            (1 << 5) /**< Has DAP register access */
#define STLINK_F_QUIRK_JTAG_DP_READ     (1 << 6) /**< Special handling for JTAG DP reads */
#define STLINK_F_HAS_AP_INIT            (1 << 7) /**< Has AP initialization */
#define STLINK_F_HAS_DPBANKSEL          (1 << 8) /**< Has DP bank selection */
#define STLINK_F_HAS_RW8_512BYTES       (1 << 9) /**< Can R/W 512 bytes at once */
/** @} */

/** @brief Additional chip feature flags */
/** @{ */
#define CHIP_F_HAS_DUAL_BANK    (1 << 0)        /**< Has dual bank flash */
#define CHIP_F_HAS_SWO_TRACING  (1 << 1)        /**< Supports SWO tracing */
/** @} */

/** @brief Debug error codes */
/** @{ */
#define STLINK_DEBUG_ERR_OK              0x80    /**< No error */
#define STLINK_DEBUG_ERR_FAULT           0x81    /**< General fault */
#define STLINK_DEBUG_ERR_WRITE           0x0c    /**< Write error */
#define STLINK_DEBUG_ERR_WRITE_VERIFY    0x0d    /**< Write verification error */
#define STLINK_DEBUG_ERR_AP_WAIT         0x10    /**< AP wait error */
#define STLINK_DEBUG_ERR_AP_FAULT        0x11    /**< AP fault */
#define STLINK_DEBUG_ERR_AP_ERROR        0x12    /**< AP error */
#define STLINK_DEBUG_ERR_DP_WAIT         0x14    /**< DP wait error */
#define STLINK_DEBUG_ERR_DP_FAULT        0x15    /**< DP fault */
#define STLINK_DEBUG_ERR_DP_ERROR        0x16    /**< DP error */
/** @} */

/** @brief Command check modes */
/** @{ */
#define CMD_CHECK_NO         0                    /**< No checking */
#define CMD_CHECK_REP_LEN    1                    /**< Check reply length */
#define CMD_CHECK_STATUS     2                    /**< Check status */
#define CMD_CHECK_RETRY      3                    /**< Check status and retry if wait error */
/** @} */

/** @brief Buffer length constants */
/** @{ */
#define C_BUF_LEN 32                             /**< Command buffer length */
#define Q_BUF_LEN (1024 * 100)                   /**< Query buffer length */
/** @} */

/** @brief The data type of STM32 Address */
typedef uint32_t stm32_addr_t;

/** @brief Enumeration of STM32 flash memory types supported by the STLink interface */
enum stm32_flash_type {
    STM32_FLASH_TYPE_UNKNOWN = 0,   /**< Unknown or unsupported flash type */
    STM32_FLASH_TYPE_C0 = 1,        /**< STM32C0 series flash memory */
    STM32_FLASH_TYPE_F0_F1_F3 = 2,  /**< STM32F0, STM32F1, and STM32F3 series flash memory */
    STM32_FLASH_TYPE_F1_XL = 3,     /**< STM32F1 XL density devices flash memory */
    STM32_FLASH_TYPE_F2_F4 = 4,     /**< STM32F2 and STM32F4 series flash memory */
    STM32_FLASH_TYPE_F7 = 5,        /**< STM32F7 series flash memory */
    STM32_FLASH_TYPE_G0 = 6,        /**< STM32G0 series flash memory */
    STM32_FLASH_TYPE_G4 = 7,        /**< STM32G4 series flash memory */
    STM32_FLASH_TYPE_H7 = 8,        /**< STM32H7 series flash memory */
    STM32_FLASH_TYPE_L0_L1 = 9,     /**< STM32L0 and STM32L1 series flash memory */
    STM32_FLASH_TYPE_L4 = 10,       /**< STM32L4 series flash memory */
    STM32_FLASH_TYPE_L5_U5_H5 = 11, /**< STM32L5, STM32U5, and STM32H5 series flash memory */
    STM32_FLASH_TYPE_WB_WL = 12,    /**< STM32WB and STM32WL series flash memory */
    STM32_FLASH_TYPE_WB0 = 13,      /**< STM32WB0 series flash memory */
};


/** @brief Enumeration of possible target device states during interaction */
enum target_state {
    TARGET_UNKNOWN = 0,       /**< Target state is unknown or cannot be determined */
    TARGET_RUNNING = 1,       /**< Target is running normally */
    TARGET_HALTED = 2,        /**< Target is halted (stopped) */
    TARGET_RESET = 3,         /**< Target is in reset state */
    TARGET_DEBUG_RUNNING = 4, /**< Target is running in debug mode */
};

/** @brief Enumeration for the different versions of the JTAG API supported by STLink devices */
enum stlink_jtag_api_version {
    STLINK_JTAG_API_V1 = 1, /**< Version 1 of the JTAG API */
    STLINK_JTAG_API_V2,     /**< Version 2 of the JTAG API */
    STLINK_JTAG_API_V3,     /**< Version 3 of the JTAG API */
};

/** @brief Feature flag indicating support for trace functionality */
#define STLINK_F_HAS_TRACE              (1 << 0)
/** @brief Feature flag indicating support for SWD frequency setting */
#define STLINK_F_HAS_SWD_SET_FREQ       (1 << 1)
/** @brief Feature flag indicating support for JTAG frequency setting */
#define STLINK_F_HAS_JTAG_SET_FREQ      (1 << 2)
/** @brief Feature flag indicating support for 16-bit memory access */
#define STLINK_F_HAS_MEM_16BIT          (1 << 3)
/** @brief Feature flag indicating support for enhanced read/write status reporting */
#define STLINK_F_HAS_GETLASTRWSTATUS2   (1 << 4)
/** @brief Feature flag indicating support for DAP register access */
#define STLINK_F_HAS_DAP_REG            (1 << 5)
/** @brief Feature flag indicating JTAG DP read quirk handling */
#define STLINK_F_QUIRK_JTAG_DP_READ     (1 << 6)
/** @brief Feature flag indicating support for AP initialization */
#define STLINK_F_HAS_AP_INIT            (1 << 7)
/** @brief Feature flag indicating support for DP bank selection */
#define STLINK_F_HAS_DPBANKSEL          (1 << 8)
/** @brief Feature flag indicating support for 512-byte read/write operations */
#define STLINK_F_HAS_RW8_512BYTES       (1 << 9)

/** @brief Structure containing version and capabilities information for an ST-Link device */
struct stlink_version_t {
    uint32_t stlink_v;                /**< ST-Link hardware version */
    uint32_t jtag_v;                  /**< JTAG protocol version */
    uint32_t swim_v;                  /**< SWIM protocol version */
    uint32_t st_vid;                  /**< ST vendor ID */
    uint32_t stlink_pid;              /**< ST-Link product ID */
    stlink_jtag_api_version jtag_api; /**< JTAG API version supported by this device */
    uint32_t flags;                   /**< Feature flags - one bit per supported feature (see STLINK_F_* macros) */
};

// C++17 additions: modern result/span types and typed enums that wrap C macros/enums.
// These live under the stlink namespace and do not change the C ABI.
#if defined(__cplusplus)
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

namespace stlink {

// Strongly-typed wrappers for common device enums.
// Values map to existing C macros/enums for painless interop.
enum class DeviceMode : int32_t {
    Dfu    = STLINK_DEV_DFU_MODE,
    Mass   = STLINK_DEV_MASS_MODE,
    Debug  = STLINK_DEV_DEBUG_MODE,
    Unknown= STLINK_DEV_UNKNOWN_MODE
};

enum class ResetType : int32_t {
    Auto          = 0,
    Hard          = 1,
    Soft          = 2,
    SoftAndHalt   = 3,
};

enum class RunType : int32_t {
    Normal       = 0,
    FlashLoader  = 1
};

// Optional: wrap target_state as a typed enum for C++ callers,
// while still allowing use of the original C enum where needed.
enum class TargetState : int32_t {
    Unknown       = TARGET_UNKNOWN,
    Running       = TARGET_RUNNING,
    Halted        = TARGET_HALTED,
    Reset         = TARGET_RESET,
    DebugRunning  = TARGET_DEBUG_RUNNING
};

// Bitflag helper for stlink_version_t::flags (STLINK_F_*).
enum class FeatureFlag : uint32_t {
    HasTrace            = STLINK_F_HAS_TRACE,
    HasSwdSetFreq       = STLINK_F_HAS_SWD_SET_FREQ,
    HasJtagSetFreq      = STLINK_F_HAS_JTAG_SET_FREQ,
    HasMem16bit         = STLINK_F_HAS_MEM_16BIT,
    HasGetLastRwStatus2 = STLINK_F_HAS_GETLASTRWSTATUS2,
    HasDapReg           = STLINK_F_HAS_DAP_REG,
    QuirkJtagDpRead     = STLINK_F_QUIRK_JTAG_DP_READ,
    HasApInit           = STLINK_F_HAS_AP_INIT,
    HasDpBankSel        = STLINK_F_HAS_DPBANKSEL,
    HasRw8_512Bytes     = STLINK_F_HAS_RW8_512BYTES
};

struct FeatureFlags {
    uint32_t bits{0};
    constexpr bool has(FeatureFlag f) const noexcept {
        return (bits & static_cast<uint32_t>(f)) != 0;
    }
    constexpr FeatureFlags with(FeatureFlag f) const noexcept {
        return FeatureFlags{ static_cast<uint32_t>(bits | static_cast<uint32_t>(f)) };
    }
};

// Lightweight byte spans (C++17).
// Use these for memory I/O without copying or exposing raw pointers everywhere.
struct ByteSpan {
    const uint8_t* data{nullptr};
    std::size_t    size{0};

    constexpr bool empty() const noexcept { return size == 0; }
    constexpr const uint8_t* begin() const noexcept { return data; }
    constexpr const uint8_t* end() const noexcept { return data + size; }
};

struct MutableByteSpan {
    uint8_t*     data{nullptr};
    std::size_t  size{0};

    constexpr bool empty() const noexcept { return size == 0; }
    constexpr uint8_t* begin() const noexcept { return data; }
    constexpr uint8_t* end() const noexcept { return data + size; }
};

// Modern, compact status and result types.
// Keep numeric code to preserve compatibility with existing error domains.
struct Status {
    int32_t       code{0};          // 0 == success; negative or non-zero == error
    const char*   category{"stlink"}; // for grouping/identification
    std::string   message{};        // optional human-readable detail

    constexpr bool ok() const noexcept { return code == 0; }

    static Status Ok() noexcept { return Status{0, "stlink", {}}; }
    static Status Error(int32_t code, std::string msg = {}, const char* cat = "stlink") {
        return Status{code, cat, std::move(msg)};
    }
};

// Expected-like result holder for C++17 (no exceptions required).
template <typename T>
struct StatusOr {
    Status                status_;
    std::optional<T>      value_;

    StatusOr() = default;
    StatusOr(Status s) : status_(std::move(s)), value_(std::nullopt) {}
    StatusOr(T v) : status_(Status::Ok()), value_(std::move(v)) {}

    bool ok() const noexcept { return status_.ok(); }
    const Status& status() const noexcept { return status_; }
    Status& status() noexcept { return status_; }
    const T& value() const { return *value_; }
    T& value() { return *value_; }
    explicit operator bool() const noexcept { return ok(); }
};

// Connection strategy (typed wrapper)
enum class ConnectType : int32_t {
    HotPlug      = 0, // CONNECT_HOT_PLUG
    Normal       = 1, // CONNECT_NORMAL
    UnderReset   = 2  // CONNECT_UNDER_RESET
};

// Typed erase type (public C++ view of erase options).
enum class EraseType : int32_t {
    NoErase = 0,
    Section = 1,
    Mass    = 2
};

// Modern C++ struct mirroring the C stlink_reg layout.
struct Reg {
    uint32_t r[16]{};
    uint32_t s[32]{};
    uint32_t xpsr{};
    uint32_t main_sp{};
    uint32_t process_sp{};
    uint32_t rw{};
    uint32_t rw2{};
    uint8_t  control{};
    uint8_t  faultmask{};
    uint8_t  basepri{};
    uint8_t  primask{};
    uint32_t fpscr{};
};

// Modern C++ struct mirroring Cortex-M3 CPU ID (cortex_m3_cpuid_t).
struct Cpuid {
    uint16_t implementer_id{};
    uint16_t variant{};
    uint16_t part{};
    uint8_t  revision{};
};

} // namespace stlink
#endif // __cplusplus


#endif // STLINK_COMMON_H