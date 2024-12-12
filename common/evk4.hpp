#pragma once

#pragma warning(disable : 4250)

#include "camera.hpp"
#include "psee.hpp"
#include "sepia.hpp"
#include <functional>
#include <iomanip>
#include <sstream>

#define sepia_evk4_bias(address, name, flags)                                                                          \
    if (force || camera_parameters.biases.name != _previous_parameters.biases.name) {                                  \
        write_register(address, (flags) | bgen_idac_ctl(camera_parameters.biases.name));                               \
    }

namespace sepia {
    namespace evk4 {
        /// get_serial reads the serial of an interface.
        std::string get_type_and_serial(sepia::usb::interface& interface) {
            interface.bulk_transfer("serial request", 0x02, {0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
            const auto buffer = interface.bulk_transfer("serial response", 0x82, std::vector<uint8_t>(16));
            std::stringstream serial;
            serial << std::hex;
            for (uint8_t index = 11; index > 7; --index) {
                serial << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(buffer[index]);
            }
            return serial.str();
        }

        /// name is the camera model.
        constexpr char name[] = "EVK4";

        /// width is the number of pixel columns.
        constexpr uint16_t width = 1280;

        /// height is the number of pixel rows.
        constexpr uint16_t height = 720;

        /// trigger_event represents a rising or falling edge on the camera's external pins.
        SEPIA_PACK(struct trigger_event {
            uint64_t t;
            uint64_t system_timestamp;
            uint8_t id;
            bool rising;
        });

        /// bias_currents lists the camera bias currents.
        struct bias_currents {
            uint8_t pr;
            uint8_t fo;
            uint8_t hpf;
            uint8_t diff_on;
            uint8_t diff;
            uint8_t diff_off;
            uint8_t inv;
            uint8_t refr;
            uint8_t reqpuy;
            uint8_t reqpux;
            uint8_t sendreqpdy;
            uint8_t unknown_1;
            uint8_t unknown_2;

            static std::vector<std::string> names() {
                return {
                    "pr",
                    "fo",
                    "hpf",
                    "diff_on",
                    "diff",
                    "diff_off",
                    "inv",
                    "refr",
                    "reqpuy",
                    "reqpux",
                    "sendreqpdy",
                    "unknown_1",
                    "unknown_2",
                };
            }

            uint8_t& by_name(const std::string& name) {
                if (name == "pr") {
                    return pr;
                }
                if (name == "fo") {
                    return fo;
                }
                if (name == "hpf") {
                    return hpf;
                }
                if (name == "diff_on") {
                    return diff_on;
                }
                if (name == "diff") {
                    return diff;
                }
                if (name == "diff_off") {
                    return diff_off;
                }
                if (name == "inv") {
                    return inv;
                }
                if (name == "refr") {
                    return refr;
                }
                if (name == "reqpuy") {
                    return reqpuy;
                }
                if (name == "reqpux") {
                    return reqpux;
                }
                if (name == "sendreqpdy") {
                    return sendreqpdy;
                }
                if (name == "unknown_1") {
                    return unknown_1;
                }
                if (name == "unknown_2") {
                    return unknown_2;
                }
                throw std::runtime_error(std::string("unknown bias name \"") + name + "\"");
            }
        };

        /// parameters lists the camera parameters.
        struct parameters {
            bias_currents biases;
            std::array<uint64_t, 20> x_mask;
            std::array<uint64_t, 12> y_mask;
            bool mask_intersection_only;
        };

        /// default_parameters provides camera parameters tuned for standard use.
        constexpr parameters default_parameters{
            {
                0x7c, // pr
                0x53, // fo
                0x00, // hpf
                0x66, // diff_on;
                0x4d, // diff;
                0x49, // diff_off;
                0x5b, // inv;
                0x14, // refr;
                0x8c, // reqpuy;
                0x7c, // reqpux;
                0x94, // sendreqpdy;
                0x74, // unknown_1;
                0x51, // unknown_2;
            },
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            false,
        };

        /// base_camera is a common base type for EVK4 cameras.
        class base_camera : public sepia::parametric_camera<parameters> {
            public:
            base_camera(const parameters& camera_parameters) :
                sepia::parametric_camera<parameters>(camera_parameters) {}
            base_camera(const base_camera&) = delete;
            base_camera(base_camera&& other) = delete;
            base_camera& operator=(const base_camera&) = delete;
            base_camera& operator=(base_camera&& other) = delete;
            virtual ~base_camera() {}
        };

        static constexpr uint32_t from_le_bytes(uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3) {
            return static_cast<uint32_t>(byte0) | (static_cast<uint32_t>(byte1) << 8)
                   | (static_cast<uint32_t>(byte2) << 16) | (static_cast<uint32_t>(byte3) << 24);
        }

        static constexpr std::array<uint8_t, 8> to_le_bytes(uint64_t value) {
            return {
                static_cast<uint8_t>(value & 0xff),
                static_cast<uint8_t>((value >> 8) & 0xff),
                static_cast<uint8_t>((value >> 16) & 0xff),
                static_cast<uint8_t>((value >> 24) & 0xff),
                static_cast<uint8_t>((value >> 32) & 0xff),
                static_cast<uint8_t>((value >> 40) & 0xff),
                static_cast<uint8_t>((value >> 48) & 0xff),
                static_cast<uint8_t>((value >> 56) & 0xff),
            };
        }

        static uint8_t reverse_bits(uint8_t byte) {
            byte = ((byte & 0xF0) >> 4) | ((byte & 0x0F) << 4);
            byte = ((byte & 0xCC) >> 2) | ((byte & 0x33) << 2);
            byte = ((byte & 0xAA) >> 1) | ((byte & 0x55) << 1);
            return byte;
        }

        /// mask_and_shift fills in register bits.
        static constexpr uint32_t mask_and_shift(uint32_t mask, uint8_t shift, uint32_t value) {
            return (value & mask) << shift;
        };
        template <uint8_t shift>
        constexpr uint32_t shift_bit() {
            return mask_and_shift(1, shift, 1u);
        }

        constexpr uint32_t reset_address = 0x400004u;

        constexpr uint32_t roi_ctrl_address = 0x0004u;
        constexpr uint32_t lifo_ctrl_address = 0x000Cu;
        constexpr uint32_t lifo_status_address = 0x0010u;
        constexpr uint32_t reserved_0014_address = 0x0014u;
        constexpr uint32_t spare0_address = 0x0018u;
        constexpr uint32_t refractory_ctrl_address = 0x0020u;
        constexpr uint32_t roi_win_ctrl_address = 0x0034u;
        constexpr uint32_t roi_win_start_addr_address = 0x0038u;
        constexpr uint32_t roi_win_end_addr_address = 0x003Cu;
        constexpr uint32_t dig_pad2_ctrl_address = 0x0044u;
        constexpr uint32_t adc_control_address = 0x004Cu;
        constexpr uint32_t adc_status_address = 0x0050u;
        constexpr uint32_t adc_misc_ctrl_address = 0x0054u;
        constexpr uint32_t temp_ctrl_address = 0x005Cu;
        constexpr uint32_t iph_mirr_ctrl_address = 0x0074u;
        constexpr uint32_t gcd_ctrl1_address = 0x0078u;
        constexpr uint32_t gcd_shadow_ctrl_address = 0x0090u;
        constexpr uint32_t gcd_shadow_status_address = 0x0094u;
        constexpr uint32_t gcd_shadow_counter_address = 0x0098u;
        constexpr uint32_t stop_sequence_control_address = 0x00C8u;
        constexpr uint32_t bias_pr_address = 0x1000u;
        constexpr uint32_t bias_fo_address = 0x1004u;
        // constexpr uint32_t bias_fo_n_address = 0x1008u;
        constexpr uint32_t bias_hpf_address = 0x100Cu;
        constexpr uint32_t bias_diff_on_address = 0x1010u;
        constexpr uint32_t bias_diff_address = 0x1014u;
        constexpr uint32_t bias_diff_off_address = 0x1018u;
        constexpr uint32_t bias_inv_address = 0x101Cu;
        constexpr uint32_t bias_refr_address = 0x1020u;
        constexpr uint32_t bias_reqpuy_address = 0x1040u;     // maybe reqpuy
        constexpr uint32_t bias_reqpux_address = 0x1044u;     // maybe reqpux
        constexpr uint32_t bias_sendreqpdy_address = 0x1048u; // maybe sendreqpdy
        constexpr uint32_t bias_unknown_1_address = 0x104Cu;
        constexpr uint32_t bias_unknown_2_address = 0x1050u;
        constexpr uint32_t bgen_ctrl_address = 0x1100u;
        constexpr uint32_t td_roi_x_begin = 0x2000u;
        constexpr uint32_t td_roi_x_end = 0x20A0u;
        constexpr uint32_t td_roi_y_begin = 0x4000u;
        constexpr uint32_t td_roi_y_end = 0x405Cu;
        constexpr uint32_t erc_reserved_6000_address = 0x6000u;
        constexpr uint32_t in_drop_rate_control_address = 0x6004u;
        constexpr uint32_t reference_period_address = 0x6008u;
        constexpr uint32_t td_target_event_rate_address = 0x600Cu;
        constexpr uint32_t erc_enable_address = 0x6028u;
        constexpr uint32_t erc_reserved_602C_address = 0x602Cu;
        constexpr uint32_t t_dropping_control_address = 0x6050u;
        constexpr uint32_t h_dropping_control_address = 0x6060u;
        constexpr uint32_t v_dropping_control_address = 0x6070u;
        constexpr uint32_t t_drop_lut_begin = 0x6400u;
        constexpr uint32_t t_drop_lut_end = 0x6800u;
        constexpr uint32_t erc_reserved_6800_6B98_begin = 0x6800u;
        constexpr uint32_t erc_reserved_6800_6B98_end = 0x6B98u;
        constexpr uint32_t edf_pipeline_control_address = 0x7000u;
        constexpr uint32_t edf_reserved_7004_address = 0x7004u;
        constexpr uint32_t readout_ctrl_address = 0x9000u;
        constexpr uint32_t ro_fsm_ctrl_address = 0x9004u;
        constexpr uint32_t time_base_ctrl_address = 0x9008u;
        constexpr uint32_t dig_ctrl_address = 0x900Cu;
        constexpr uint32_t dig_start_pos_address = 0x9010u;
        constexpr uint32_t dig_end_pos_address = 0x9014u;
        constexpr uint32_t ro_ctrl_address = 0x9028u;
        constexpr uint32_t area_x0_addr_address = 0x902Cu;
        constexpr uint32_t area_x1_addr_address = 0x9030u;
        constexpr uint32_t area_x2_addr_address = 0x9034u;
        constexpr uint32_t area_x3_addr_address = 0x9038u;
        constexpr uint32_t area_x4_addr_address = 0x903Cu;
        constexpr uint32_t area_y0_addr_address = 0x9040u;
        constexpr uint32_t area_y1_addr_address = 0x9044u;
        constexpr uint32_t area_y2_addr_address = 0x9048u;
        constexpr uint32_t area_y3_addr_address = 0x904Cu;
        constexpr uint32_t area_y4_addr_address = 0x9050u;
        constexpr uint32_t counter_ctrl_address = 0x9054u;
        constexpr uint32_t counter_timer_threshold_address = 0x9058u;
        constexpr uint32_t digital_mask_pixel_00_address = 0x9100u;
        constexpr uint32_t digital_mask_pixel_01_address = 0x9104u;
        constexpr uint32_t digital_mask_pixel_02_address = 0x9108u;
        constexpr uint32_t digital_mask_pixel_03_address = 0x910Cu;
        constexpr uint32_t digital_mask_pixel_04_address = 0x9110u;
        constexpr uint32_t digital_mask_pixel_05_address = 0x9114u;
        constexpr uint32_t digital_mask_pixel_06_address = 0x9118u;
        constexpr uint32_t digital_mask_pixel_07_address = 0x911Cu;
        constexpr uint32_t digital_mask_pixel_08_address = 0x9120u;
        constexpr uint32_t digital_mask_pixel_09_address = 0x9124u;
        constexpr uint32_t digital_mask_pixel_10_address = 0x9128u;
        constexpr uint32_t digital_mask_pixel_11_address = 0x912Cu;
        constexpr uint32_t digital_mask_pixel_12_address = 0x9130u;
        constexpr uint32_t digital_mask_pixel_13_address = 0x9134u;
        constexpr uint32_t digital_mask_pixel_14_address = 0x9138u;
        constexpr uint32_t digital_mask_pixel_15_address = 0x913Cu;
        constexpr uint32_t digital_mask_pixel_16_address = 0x9140u;
        constexpr uint32_t digital_mask_pixel_17_address = 0x9144u;
        constexpr uint32_t digital_mask_pixel_18_address = 0x9148u;
        constexpr uint32_t digital_mask_pixel_19_address = 0x914Cu;
        constexpr uint32_t digital_mask_pixel_20_address = 0x9150u;
        constexpr uint32_t digital_mask_pixel_21_address = 0x9154u;
        constexpr uint32_t digital_mask_pixel_22_address = 0x9158u;
        constexpr uint32_t digital_mask_pixel_23_address = 0x915Cu;
        constexpr uint32_t digital_mask_pixel_24_address = 0x9160u;
        constexpr uint32_t digital_mask_pixel_25_address = 0x9164u;
        constexpr uint32_t digital_mask_pixel_26_address = 0x9168u;
        constexpr uint32_t digital_mask_pixel_27_address = 0x916Cu;
        constexpr uint32_t digital_mask_pixel_28_address = 0x9170u;
        constexpr uint32_t digital_mask_pixel_29_address = 0x9174u;
        constexpr uint32_t digital_mask_pixel_30_address = 0x9178u;
        constexpr uint32_t digital_mask_pixel_31_address = 0x917Cu;
        constexpr uint32_t digital_mask_pixel_32_address = 0x9180u;
        constexpr uint32_t digital_mask_pixel_33_address = 0x9184u;
        constexpr uint32_t digital_mask_pixel_34_address = 0x9188u;
        constexpr uint32_t digital_mask_pixel_35_address = 0x918Cu;
        constexpr uint32_t digital_mask_pixel_36_address = 0x9190u;
        constexpr uint32_t digital_mask_pixel_37_address = 0x9194u;
        constexpr uint32_t digital_mask_pixel_38_address = 0x9198u;
        constexpr uint32_t digital_mask_pixel_39_address = 0x919Cu;
        constexpr uint32_t digital_mask_pixel_40_address = 0x91A0u;
        constexpr uint32_t digital_mask_pixel_41_address = 0x91A4u;
        constexpr uint32_t digital_mask_pixel_42_address = 0x91A8u;
        constexpr uint32_t digital_mask_pixel_43_address = 0x91ACu;
        constexpr uint32_t digital_mask_pixel_44_address = 0x91B0u;
        constexpr uint32_t digital_mask_pixel_45_address = 0x91B4u;
        constexpr uint32_t digital_mask_pixel_46_address = 0x91B8u;
        constexpr uint32_t digital_mask_pixel_47_address = 0x91BCu;
        constexpr uint32_t digital_mask_pixel_48_address = 0x91C0u;
        constexpr uint32_t digital_mask_pixel_49_address = 0x91C4u;
        constexpr uint32_t digital_mask_pixel_50_address = 0x91C8u;
        constexpr uint32_t digital_mask_pixel_51_address = 0x91CCu;
        constexpr uint32_t digital_mask_pixel_52_address = 0x91D0u;
        constexpr uint32_t digital_mask_pixel_53_address = 0x91D4u;
        constexpr uint32_t digital_mask_pixel_54_address = 0x91D8u;
        constexpr uint32_t digital_mask_pixel_55_address = 0x91DCu;
        constexpr uint32_t digital_mask_pixel_56_address = 0x91E0u;
        constexpr uint32_t digital_mask_pixel_57_address = 0x91E4u;
        constexpr uint32_t digital_mask_pixel_58_address = 0x91E8u;
        constexpr uint32_t digital_mask_pixel_59_address = 0x91ECu;
        constexpr uint32_t digital_mask_pixel_60_address = 0x91F0u;
        constexpr uint32_t digital_mask_pixel_61_address = 0x91F4u;
        constexpr uint32_t digital_mask_pixel_62_address = 0x91F8u;
        constexpr uint32_t digital_mask_pixel_63_address = 0x91FCu;
        constexpr uint32_t area_cnt00_address = 0x9200u;
        constexpr uint32_t area_cnt01_address = 0x9204u;
        constexpr uint32_t area_cnt02_address = 0x9208u;
        constexpr uint32_t area_cnt03_address = 0x920Cu;
        constexpr uint32_t area_cnt04_address = 0x9210u;
        constexpr uint32_t area_cnt05_address = 0x9214u;
        constexpr uint32_t area_cnt06_address = 0x9218u;
        constexpr uint32_t area_cnt07_address = 0x921Cu;
        constexpr uint32_t area_cnt08_address = 0x9220u;
        constexpr uint32_t area_cnt09_address = 0x9224u;
        constexpr uint32_t area_cnt10_address = 0x9228u;
        constexpr uint32_t area_cnt11_address = 0x922Cu;
        constexpr uint32_t area_cnt12_address = 0x9230u;
        constexpr uint32_t area_cnt13_address = 0x9234u;
        constexpr uint32_t area_cnt14_address = 0x9238u;
        constexpr uint32_t area_cnt15_address = 0x923Cu;
        constexpr uint32_t evt_vector_cnt_val_address = 0x9244u;
        constexpr uint32_t mipi_control_address = 0xB000u;
        constexpr uint32_t mipi_packet_size_address = 0xB020u;
        constexpr uint32_t mipi_packet_timeout_address = 0xB024u;
        constexpr uint32_t mipi_frame_period_address = 0xB028u;
        constexpr uint32_t mipi_frame_blanking_address = 0xB030u;
        constexpr uint32_t afk_pipeline_control_address = 0xC000u;
        constexpr uint32_t reserved_C004_address = 0xC004u;
        constexpr uint32_t filter_period_address = 0xC008u;
        constexpr uint32_t invalidation_address = 0xC0C0u;
        constexpr uint32_t afk_initialization_address = 0xC0C4u;
        constexpr uint32_t stc_pipeline_control_address = 0xD000u;
        constexpr uint32_t stc_param_address = 0xD004u;
        constexpr uint32_t trail_param_address = 0xD008u;
        constexpr uint32_t timestamping_address = 0xD00Cu;
        constexpr uint32_t reserved_D0C0_address = 0xD0C0u;
        constexpr uint32_t stc_initialization_address = 0xD0C4u;
        constexpr uint32_t slvs_control_address = 0xE000u;
        constexpr uint32_t slvs_packet_size_address = 0xE020u;
        constexpr uint32_t slvs_packet_timeout_address = 0xE024u;
        constexpr uint32_t slvs_frame_blanking_address = 0xE030u;
        constexpr uint32_t slvs_phy_logic_ctrl_00_address = 0xE150u;

        // bias generation
        auto bgen_idac_ctl = std::bind(mask_and_shift, 0b11111111u, 0, std::placeholders::_1);
        auto bgen_vdac_ctl = std::bind(mask_and_shift, 0b11111111u, 8, std::placeholders::_1);
        auto bgen_buf_stg = std::bind(mask_and_shift, 0b111u, 16, std::placeholders::_1);
        constexpr auto bgen_ibtype_sel = shift_bit<19>();
        constexpr auto begn_mux_sel = shift_bit<20>();
        constexpr auto bgen_mux_en = shift_bit<21>();
        constexpr auto bgen_vdac_en = shift_bit<22>();
        constexpr auto bgen_buf_en = shift_bit<23>();
        constexpr auto bgen_idac_en = shift_bit<24>();
        auto bgen_reserved = std::bind(mask_and_shift, 0b111u, 25, std::placeholders::_1);
        constexpr auto bgen_single = shift_bit<28>();
        constexpr auto bgenctrl_burst_transfer_bank_0 = shift_bit<0>();
        constexpr auto bgenctrl_burst_transfer_bank_1 = shift_bit<1>();
        constexpr auto bgenctrl_bias_rstn = shift_bit<2>();
        constexpr auto bgenspr_enable = shift_bit<0>();
        auto bgenspr_value = std::bind(mask_and_shift, 0b111111111111111u, 1, std::placeholders::_1);

        // clock control
        constexpr auto clk_control_core_en = shift_bit<0>();
        constexpr auto clk_control_core_soft_rst = shift_bit<1>();
        constexpr auto clk_control_core_reg_bank_rst = shift_bit<2>();
        constexpr auto clk_control_sensor_if_en = shift_bit<4>();
        constexpr auto clk_control_sensor_if_soft_rst = shift_bit<5>();
        constexpr auto clk_control_sensor_if_reg_bank_rst = shift_bit<6>();
        constexpr auto clk_control_host_if_en = shift_bit<8>();
        constexpr auto clk_control_host_if_soft_rst = shift_bit<9>();
        constexpr auto clk_control_host_if_reg_bank_rst = shift_bit<10>();

        // LIFO configuration (global absolute illumination)
        // constexpr auto lifo_pad_en = shift_bit<0>();
        // constexpr auto lifo_px_array_en = shift_bit<1>();
        // constexpr auto lifo_calib_en = shift_bit<2>();
        // constexpr auto lifo_calib_x10_en = shift_bit<3>();
        // constexpr auto lifo_out_en = shift_bit<4>();

        // LIFO control (global absolute illumination)
        // auto lifo_ctrl_counter = std::bind(mask_and_shift, 0x3ffffff, 0, std::placeholders::_1);
        // constexpr auto lifo_ctrl_counter_valid = shift_bit<30>();
        // constexpr auto lifo_ctrl_cnt_en = shift_bit<30>();
        // constexpr auto lifo_ctrl_en = shift_bit<31>();

        // pads control
        auto dig_pad_ctrl_miso_drive_strength = std::bind(mask_and_shift, 0b11, 0, std::placeholders::_1);
        auto dig_pad_ctrl_cam_sync_drive_strength = std::bind(mask_and_shift, 0b11, 2, std::placeholders::_1);
        auto dig_pad_ctrl_gpio1_drive_strength = std::bind(mask_and_shift, 0b11, 4, std::placeholders::_1);
        auto dig_pad_ctrl_gpio2_drive_strength = std::bind(mask_and_shift, 0b11, 6, std::placeholders::_1);
        auto dig_pad_ctrl_d_drive_strength = std::bind(mask_and_shift, 0b11, 8, std::placeholders::_1);
        auto dig_pad_ctrl_tpd_drive_strength = std::bind(mask_and_shift, 0b11, 10, std::placeholders::_1);

        // event data formatter
        constexpr auto edf_control_format_2_0 = 0u;
        constexpr auto edf_control_format_3_0 = 1u;
        constexpr auto edf_pipeline_control_enable = shift_bit<0>();
        constexpr auto edf_pipeline_control_bypass = shift_bit<1>();
        constexpr auto edf_pipeline_control_preserve_last = shift_bit<2>();
        auto edf_pipeline_control_bypass_mem = std::bind(mask_and_shift, 0b11, 3, std::placeholders::_1);
        auto edf_pipeline_control_timeout = std::bind(mask_and_shift, 0xffffu, 16, std::placeholders::_1);

        // event output interface
        constexpr auto eoi_mode_control_enable = shift_bit<0>();
        constexpr auto eoi_mode_control_bypass_enable = shift_bit<1>();
        constexpr auto eoi_mode_control_packet_enable = shift_bit<2>();
        constexpr auto eoi_mode_control_short_latency_enable = shift_bit<3>();
        constexpr auto eoi_mode_control_short_latency_skip_enable = shift_bit<4>();
        auto eoi_mode_control_unpacking_byte_order = std::bind(mask_and_shift, 0b11, 5, std::placeholders::_1);
        constexpr auto eoi_mode_control_gpif_like_enable = shift_bit<7>();
        constexpr auto eoi_mode_control_fifo_bypass = shift_bit<8>();
        constexpr auto eoi_mode_control_clk_out_en = shift_bit<9>();
        constexpr auto eoi_mode_control_clk_out_pol = shift_bit<10>();
        auto eoi_mode_control_self_test = std::bind(mask_and_shift, 0b11, 11, std::placeholders::_1);
        constexpr auto eoi_mode_control_out_gating_disable = shift_bit<13>();

        // event rate control
        constexpr auto erc_drop_rate_event_delay_fifo_en = shift_bit<0>();
        constexpr auto erc_drop_rate_manual_en = shift_bit<1>();
        auto erc_drop_rate_manual_td_value = std::bind(mask_and_shift, 0b11111, 2, std::placeholders::_1);
        auto erc_drop_rate_manual_em_value = std::bind(mask_and_shift, 0b11111, 7, std::placeholders::_1);
        auto erc_em_target_event_rate_val = std::bind(mask_and_shift, 0x003fffffu, 0, std::placeholders::_1);
        constexpr auto erc_h_dropping_control_en = shift_bit<0>();
        auto erc_h_dropping_lut_v0 = std::bind(mask_and_shift, 0b11111u, 0, std::placeholders::_1);
        auto erc_h_dropping_lut_v1 = std::bind(mask_and_shift, 0b11111u, 8, std::placeholders::_1);
        auto erc_h_dropping_lut_v2 = std::bind(mask_and_shift, 0b11111u, 16, std::placeholders::_1);
        auto erc_h_dropping_lut_v3 = std::bind(mask_and_shift, 0b11111u, 24, std::placeholders::_1);
        constexpr auto erc_pipeline_control_en = shift_bit<0>();
        constexpr auto erc_pipeline_control_bypass = shift_bit<1>();
        constexpr auto erc_pipeline_control_fifo0_bypass = shift_bit<2>();
        constexpr auto erc_pipeline_control_fifo1_bypass = shift_bit<3>();
        constexpr auto erc_pipeline_control_fifo2_bypass = shift_bit<4>();
        auto erc_reference_period_val = std::bind(mask_and_shift, 0b1111111111u, 0, std::placeholders::_1);
        auto erc_pong_drop_interest_v0 = std::bind(mask_and_shift, 0b111111u, 0, std::placeholders::_1);
        auto erc_pong_drop_interest_v1 = std::bind(mask_and_shift, 0b111111u, 8, std::placeholders::_1);
        auto erc_pong_drop_interest_v2 = std::bind(mask_and_shift, 0b111111u, 16, std::placeholders::_1);
        auto erc_pong_drop_interest_v3 = std::bind(mask_and_shift, 0b111111u, 24, std::placeholders::_1);
        constexpr auto erc_refine_drop_rate_td_en = shift_bit<0>();
        constexpr auto erc_refine_drop_rate_interest_level_td_en = shift_bit<1>();
        constexpr auto erc_refine_drop_rate_out_fb_td_en = shift_bit<2>();
        constexpr auto erc_refine_drop_rate_em_en = shift_bit<3>();
        constexpr auto erc_refine_drop_rate_interest_level_em_en = shift_bit<4>();
        constexpr auto erc_refine_drop_rate_out_fb_em_en = shift_bit<5>();
        auto erc_select_pong_grid_mem_val = std::bind(mask_and_shift, 0b11u, 0, std::placeholders::_1);
        constexpr auto erc_t_dropping_control_en = shift_bit<0>();
        auto erc_t_dropping_lut_v0 = std::bind(mask_and_shift, 0b11111u, 0, std::placeholders::_1);
        auto erc_t_dropping_lut_v1 = std::bind(mask_and_shift, 0b11111u, 8, std::placeholders::_1);
        auto erc_t_dropping_lut_v2 = std::bind(mask_and_shift, 0b11111u, 16, std::placeholders::_1);
        auto erc_t_dropping_lut_v3 = std::bind(mask_and_shift, 0b11111u, 24, std::placeholders::_1);
        auto erc_td_target_event_rate_val = std::bind(mask_and_shift, 0x003fffffu, 0, std::placeholders::_1);
        constexpr auto erc_v_dropping_control_en = shift_bit<0>();
        auto erc_v_dropping_lut_v0 = std::bind(mask_and_shift, 0b11111u, 0, std::placeholders::_1);
        auto erc_v_dropping_lut_v1 = std::bind(mask_and_shift, 0b11111u, 8, std::placeholders::_1);
        auto erc_v_dropping_lut_v2 = std::bind(mask_and_shift, 0b11111u, 16, std::placeholders::_1);
        auto erc_v_dropping_lut_v3 = std::bind(mask_and_shift, 0b11111u, 24, std::placeholders::_1);

        // event formatter
        constexpr auto evt_data_formatter_control_enable = shift_bit<0>();
        constexpr auto evt_data_formatter_control_bypass = shift_bit<1>();
        constexpr auto evt_merge_control_enable = shift_bit<0>();
        constexpr auto evt_merge_control_bypass = shift_bit<1>();
        constexpr auto evt_merge_control_source = shift_bit<2>();

        // external triggers
        constexpr auto ext_trigger_0_enable = shift_bit<0>();
        constexpr auto ext_trigger_1_enable = shift_bit<1>();
        constexpr auto ext_trigger_2_enable = shift_bit<2>();
        constexpr auto ext_trigger_3_enable = shift_bit<3>();
        constexpr auto ext_trigger_4_enable = shift_bit<4>();
        constexpr auto ext_trigger_5_enable = shift_bit<5>();
        constexpr auto ext_trigger_6_enable = shift_bit<6>();

        // FPGA control
        constexpr auto fpga_ctrl_enable = shift_bit<0>();
        constexpr auto fpga_ctrl_bypass = shift_bit<1>();
        constexpr auto fpga_ctrl_blocking_mode = shift_bit<2>();
        constexpr auto fpga_ctrl_arst_n = shift_bit<3>();
        constexpr auto fpga_ctrl_td_arst_n = shift_bit<4>();
        constexpr auto fpga_ctrl_em_arst_n = shift_bit<5>();
        constexpr auto fpga_ctrl_test_mode = shift_bit<6>();
        constexpr auto fpga_ctrl_ldo_vdda_en = shift_bit<7>();
        constexpr auto fpga_ctrl_ldo_vddc_en = shift_bit<8>();
        constexpr auto fpga_ctrl_ldo_vddio_en = shift_bit<9>();
        constexpr auto fpga_ctrl_ldo_vneg_en = shift_bit<10>();
        constexpr auto fpga_ctrl_last_ctrl_mode = shift_bit<11>();
        constexpr auto fpga_ctrl_half_word_swap = shift_bit<12>();
        constexpr auto fpga_ctrl_sensor_clk_en = shift_bit<13>();
        constexpr auto fpga_ctrl_cam_synci_reg = shift_bit<14>();
        constexpr auto fpga_ctrl_ext_trig_reg = shift_bit<15>();
        constexpr auto fpga_ctrl_sensor_clkfreq_ctl = shift_bit<16>();
        constexpr auto fpga_ctrl_sensor_ready_ff = shift_bit<18>();
        constexpr auto fpga_ctrl_clkfreq_vld = shift_bit<19>();
        constexpr auto fpga_ctrl_td_pol_inv = shift_bit<20>();
        constexpr auto fpga_ctrl_em_pol_inv = shift_bit<21>();
        constexpr auto fpga_ctrl_gen_last = shift_bit<22>();

        // global sensor control
        constexpr auto global_ctrl_px_em_couple_ctrl = shift_bit<0>();
        constexpr auto global_ctrl_analog_rstn = shift_bit<1>();
        constexpr auto global_ctrl_erc_self_test_en = shift_bit<2>();

        // low-dropout regulator (power control)
        constexpr auto ldo_ana_en = shift_bit<0>();
        constexpr auto ldo_ana_en_limit = shift_bit<1>();
        constexpr auto ldo_ana_ron = shift_bit<2>();
        constexpr auto ldo_ana_thro = shift_bit<3>();
        constexpr auto ldo_ana_climit1 = shift_bit<4>();
        constexpr auto ldo_ana_climit2 = shift_bit<5>();
        auto ldo_ana_adj = std::bind(mask_and_shift, 0b1111u, 6, std::placeholders::_1);
        auto ldo_ana_comp = std::bind(mask_and_shift, 0b11u, 10, std::placeholders::_1);
        constexpr auto ldo_ana_en_ref = shift_bit<12>();
        constexpr auto ldo_ana_indicator = shift_bit<13>();
        constexpr auto ldo_bg_en = shift_bit<0>();
        constexpr auto ldo_bg_bypass = shift_bit<1>();
        auto ldo_bg_adj = std::bind(mask_and_shift, 0b111u, 2, std::placeholders::_1);
        constexpr auto ldo_bg_th = shift_bit<5>();
        constexpr auto ldo_bg_chk = shift_bit<6>();
        constexpr auto ldo_bg_indicator = shift_bit<7>();
        constexpr auto ldo_bg2_en = shift_bit<0>();
        constexpr auto ldo_bg2_bypass = shift_bit<1>();
        auto ldo_bg2_adj = std::bind(mask_and_shift, 0b111u, 2, std::placeholders::_1);
        constexpr auto ldo_bg2_th = shift_bit<5>();
        constexpr auto ldo_bg2_chk = shift_bit<6>();
        constexpr auto ldo_bg2_indicator = shift_bit<7>();
        auto ldo_cc_dft_cnt = std::bind(mask_and_shift, 0b1111u, 0, std::placeholders::_1);
        auto ldo_cc_inres_adj = std::bind(mask_and_shift, 0b1111u, 4, std::placeholders::_1);
        constexpr auto ldo_cc_en = shift_bit<8>();
        constexpr auto ldo_cc_extres_en = shift_bit<9>();
        constexpr auto ldo_cc_extres_enh = shift_bit<10>();
        constexpr auto ldo_dig_en = shift_bit<0>();
        constexpr auto ldo_dig_en_limit = shift_bit<1>();
        constexpr auto ldo_dig_psf = shift_bit<2>();
        constexpr auto ldo_dig_ron = shift_bit<3>();
        constexpr auto ldo_dig_thro = shift_bit<4>();
        constexpr auto ldo_dig_climit100ma = shift_bit<5>();
        constexpr auto ldo_dig_climit600ma = shift_bit<6>();
        auto ldo_dig_adj = std::bind(mask_and_shift, 0b1111u, 7, std::placeholders::_1);
        auto ldo_dig_comp = std::bind(mask_and_shift, 0b11u, 11, std::placeholders::_1);
        constexpr auto ldo_dig_en_ref = shift_bit<13>();
        constexpr auto ldo_dig_indicator = shift_bit<14>();
        constexpr auto ldo_dig_en_delay = shift_bit<15>();
        constexpr auto ldo_dig_start_pulse = shift_bit<16>();
        constexpr auto ldo_pix_en = shift_bit<0>();
        constexpr auto ldo_pix_en_limit = shift_bit<1>();
        constexpr auto ldo_pix_ron = shift_bit<2>();
        constexpr auto ldo_pix_thro = shift_bit<3>();
        auto ldo_pix_adj = std::bind(mask_and_shift, 0b111u, 4, std::placeholders::_1);
        constexpr auto ldo_pix_climit1 = shift_bit<7>();
        constexpr auto ldo_pix_climit2 = shift_bit<8>();
        constexpr auto ldo_pix_en_ref = shift_bit<9>();
        constexpr auto ldo_pix_indicator = shift_bit<10>();
        constexpr auto ldo_pix_start_pulse = shift_bit<11>();

        // analog readout
        constexpr auto readout_ctrl_test_pixel_mux_en = shift_bit<0>();
        constexpr auto readout_ctrl_td_self_test_en = shift_bit<2>();
        constexpr auto readout_ctrl_em_self_test_en = shift_bit<3>();
        constexpr auto readout_ctrl_analog_pipe_en = shift_bit<4>();
        auto ro_act_pdy_drive = std::bind(mask_and_shift, 0b111u, 0, std::placeholders::_1);
        auto ro_act_puy_drive = std::bind(mask_and_shift, 0b111u, 3, std::placeholders::_1);
        constexpr auto ro_sendreq_y_stat_en = shift_bit<7>();
        constexpr auto ro_sendreq_y_rstn = shift_bit<8>();
        constexpr auto ro_int_x_rstn = shift_bit<9>();
        constexpr auto ro_int_y_rstn = shift_bit<10>();
        constexpr auto ro_int_x_stat_en = shift_bit<11>();
        constexpr auto ro_int_y_stat_en = shift_bit<12>();
        constexpr auto ro_addr_y_stat_en = shift_bit<13>();
        constexpr auto ro_addr_y_rstn = shift_bit<14>();
        constexpr auto ro_ack_y_rstn = shift_bit<15>();
        constexpr auto ro_ack_y_rstn_pol = shift_bit<16>();
        constexpr auto ro_arb_y_rstn = shift_bit<17>();

        // region of interest
        constexpr auto roi_ctrl_em_en = shift_bit<0>();
        constexpr auto roi_ctrl_td_en = shift_bit<1>();
        constexpr auto roi_ctrl_em_shadow_trigger = shift_bit<4>();
        constexpr auto roi_ctrl_td_shadow_trigger = shift_bit<5>();
        constexpr auto roi_ctrl_td_roni_n_en = shift_bit<6>();
        constexpr auto roi_ctrl_em_scan_en = shift_bit<7>();
        constexpr auto roi_ctrl_td_scan_en = shift_bit<8>();
        constexpr auto roi_ctrl_px_em_rstn = shift_bit<9>();
        constexpr auto roi_ctrl_px_td_rstn = shift_bit<10>();
        auto roi_ctrl_td_scan_timer = std::bind(mask_and_shift, 0b1111111u, 11, std::placeholders::_1);
        auto roi_ctrl_em_scan_timer = std::bind(mask_and_shift, 0b1111111u, 18, std::placeholders::_1);
        auto roi_ctrl_pix_slope_n_ctl = std::bind(mask_and_shift, 0b11u, 28, std::placeholders::_1);
        auto roi_ctrl_pix_slope_p_ctl = std::bind(mask_and_shift, 0b11u, 30, std::placeholders::_1);

        // test bus
        constexpr auto test_bus_ctrl_tp_buf_en = shift_bit<1>();
        constexpr auto test_bus_ctrl_tp_3t_aps_en = shift_bit<2>();
        constexpr auto test_bus_ctrl_tp_ro_buf_en = shift_bit<3>();
        constexpr auto test_bus_ctrl_tp_ro_buf_sel = shift_bit<4>();
        auto test_bus_ctrl_tbus_sel_tpa1 = std::bind(mask_and_shift, 0b1111u, 8, std::placeholders::_1);
        auto test_bus_ctrl_tbus_sel_tpa2 = std::bind(mask_and_shift, 0b1111u, 12, std::placeholders::_1);
        auto test_bus_ctrl_tbus_sel_tpa3 = std::bind(mask_and_shift, 0b1111u, 16, std::placeholders::_1);
        auto test_bus_ctrl_tbus_sel_tpa4 = std::bind(mask_and_shift, 0b1111u, 20, std::placeholders::_1);
        constexpr auto test_bus_ctrl_px_scan_bot = shift_bit<24>();
        constexpr auto test_bus_ctrl_px_scan_top = shift_bit<25>();
        constexpr auto test_bus_ctrl_px_scan_left = shift_bit<26>();
        constexpr auto test_bus_ctrl_px_scan_right = shift_bit<27>();
        constexpr auto test_bus_ctrl_px_scan_pol_sel = shift_bit<28>();

        // th(reshold)?
        constexpr auto th_recovery_control_enable = shift_bit<0>();
        constexpr auto th_recovery_control_bypass = shift_bit<1>();

        // analog time base control
        constexpr auto time_base_ctrl_enable = shift_bit<0>();
        constexpr auto time_base_ctrl_mode = shift_bit<1>();
        constexpr auto time_base_ctrl_external_mode = shift_bit<2>();
        constexpr auto time_base_ctrl_external_mode_enable = shift_bit<3>();
        auto time_base_ctrl_us_counter_max = std::bind(mask_and_shift, 0b1111111u, 4, std::placeholders::_1);

        // digital time base control
        constexpr auto time_base_mode_enable = shift_bit<0>();
        constexpr auto time_base_mode_ext_sync_mode = shift_bit<1>();
        constexpr auto time_base_mode_ext_sync_enable = shift_bit<2>();
        constexpr auto time_base_mode_ext_sync_master = shift_bit<3>();
        constexpr auto time_base_mode_ext_sync_master_sel = shift_bit<4>();
        constexpr auto time_base_mode_enable_ext_sync = shift_bit<5>();
        constexpr auto time_base_mode_enable_cam_sync = shift_bit<6>();

        /// buffered_camera is a buffered observable connected to an EVK4 camera.
        template <typename HandleBuffer, typename HandleException>
        class buffered_camera : public base_camera, public sepia::buffered_camera<HandleBuffer, HandleException> {
            public:
            buffered_camera(
                HandleBuffer&& handle_buffer,
                HandleException&& handle_exception,
                const parameters& camera_parameters = default_parameters,
                const std::string& serial = {},
                const std::chrono::steady_clock::duration& timeout = std::chrono::milliseconds(100),
                std::size_t buffers_count = 32,
                std::size_t fifo_size = 4096,
                std::function<void()> handle_drop = []() {}) :
                base_camera(camera_parameters),
                sepia::buffered_camera<HandleBuffer, HandleException>(
                    std::forward<HandleBuffer>(handle_buffer),
                    std::forward<HandleException>(handle_exception),
                    timeout,
                    fifo_size,
                    std::move(handle_drop)),
                _active_transfers(buffers_count) {
                _interface = usb::open(name, psee::identities, psee::get_type_and_serial, serial);
                _interface.checked_control_transfer(
                    "control0", 0x80, 0x06, 0x0300, 0x0000, std::vector<uint8_t>({0x04, 0x03, 0x09, 0x04}), 1000);
                // control 1
                {
                    auto buffer = std::vector<uint8_t>(24, 0);
                    const auto size =
                        _interface.unchecked_control_transfer("control1", 0x80, 0x06, 0x0301, 0x0409, buffer, 1000);
                    auto valid = false;
                    if (!valid) {
                        const auto signature =
                            std::vector<uint8_t>({0x14, 0x03, 'P', 0x00, 'r', 0x00, 'o', 0x00, 'p', 0x00,
                                                  'h',  0x00, 'e', 0x00, 's', 0x00, 'e', 0x00, 'e', 0x00});
                        if (size >= signature.size()) {
                            valid = true;
                            for (std::size_t index = 0; index < signature.size(); ++index) {
                                valid = signature[index] == buffer[index];
                                if (!valid) {
                                    break;
                                }
                            }
                        }
                    }
                    if (!valid) {
                        const auto signature =
                            std::vector<uint8_t>({0x18, 0x03, 'C', 0x00, 'e', 0x00, 'n', 0x00, 't', 0x00, 'u', 0x00,
                                                  'r',  0x00, 'y', 0x00, 'A', 0x00, 'r', 0x00, 'k', 0x00, 's', 0x00});
                        if (size >= signature.size()) {
                            valid = true;
                            for (std::size_t index = 0; index < signature.size(); ++index) {
                                valid = signature[index] == buffer[index];
                                if (!valid) {
                                    break;
                                }
                            }
                        }
                    }
                    if (!valid) {
                        std::stringstream control1;
                        control1 << "Unexpect control1 register contents (";
                        for (uint32_t index = 0; index < size; ++index) {
                            control1 << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(buffer[index]);
                        }
                        control1 << ")";
                        throw std::runtime_error(control1.str());
                    }
                }
                _interface.checked_control_transfer(
                    "control0",
                    0x80,
                    0x06,
                    0x0300,
                    0x0000,
                    std::vector<uint8_t>({0x04, 0x03, 0x09, 0x04}),
                    1000); // potentially redundant

                // control 2
                {
                    auto buffer = std::vector<uint8_t>(50, 0);
                    const auto size =
                        _interface.unchecked_control_transfer("control2", 0x80, 0x06, 0x0302, 0x0409, buffer, 1000);
                    auto valid = false;
                    if (!valid) {
                        const auto signature =
                            std::vector<uint8_t>({0x0a, 0x03, 'E', 0x00, 'V', 0x00, 'K', 0x00, '4', 0x00});
                        if (size >= signature.size()) {
                            valid = true;
                            for (std::size_t index = 0; index < signature.size(); ++index) {
                                valid = signature[index] == buffer[index];
                                if (!valid) {
                                    break;
                                }
                            }
                        }
                    }
                    if (!valid) {
                        const auto signature = std::vector<uint8_t>({
                            0x32, 0x03, 'S',  0x00, 'i',  0x00, 'l',  0x00, 'k',  0x00, 'y',  0x00, 'E',
                            0x00, 'v',  0x00, 'C',  0x00, 'a',  0x00, 'm',  0x00, ' ',  0x00, 'H',  0x00,
                            'D',  0x00, ' ',  0x00, 'v',  0x00, '0',  0x00, '3',  0x00, '.',  0x00, '0',
                            0x00, '9',  0x00, '.',  0x00, '0',  0x00, '0',  0x00, 'C',  0x00,
                        });
                        if (size >= signature.size()) {
                            valid = true;
                            for (std::size_t index = 0; index < signature.size(); ++index) {
                                valid = signature[index] == buffer[index];
                                if (!valid) {
                                    break;
                                }
                            }
                        }
                    }
                    if (!valid) {
                        std::stringstream control1;
                        control1 << "Unexpect control2 register contents (";
                        for (uint32_t index = 0; index < size; ++index) {
                            control1 << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(buffer[index]);
                        }
                        control1 << ")";
                        throw std::runtime_error(control1.str());
                    }
                }
                bulk_request({0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 1000); // read release version
                bulk_request({0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 1000); // read build date
                bulk_request({0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1000); // ?
                bulk_request(
                    {0x03, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                    1000);                            // psee,ccam5_imx636 psee,ccam5_gen42
                read_register(reserved_0014_address); // ?
                bulk_request({0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 1000); // serial request
                bulk_request({0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1000); // ?
                bulk_request(
                    {0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                    1000); // CCam5 Imx636 Event-Based Camera
                bulk_request(
                    {0x03, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                    1000);                            // psee,ccam5_imx636 psee,ccam5_gen42
                read_register(reserved_0014_address); // ?

                // issd_evk3_imx636_stop in hal_psee_plugins/include/devices/imx636/imx636_evk3_issd.h {
                write_register(roi_ctrl_address, 0xf0005042u);
                write_register(0x002c, 0x0022c324u); // unknown address
                write_register(ro_ctrl_address, 0x00000002u);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                read_register(time_base_ctrl_address);
                write_register(time_base_ctrl_address, 0x00000644u);
                write_register(mipi_control_address, 0x000002f8u);
                std::this_thread::sleep_for(std::chrono::microseconds(300));
                // }

                // issd_evk3_imx636_destroy in hal_psee_plugins/include/devices/imx636/imx636_evk3_issd.h {
                write_register(0x0070, 0x00400008u); // unknown address
                write_register(0x006c, 0x0ee47114u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                write_register(0xa00c, 0x00020400u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                write_register(0xa010, 0x00008068u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0x1104, 0x00000000u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa020, 0x00000050u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa004, 0x000b0500u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa008, 0x00002404u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa000, 0x000b0500u); // unknown address
                write_register(0xb044, 0x00000000u); // unknown address
                write_register(0xb004, 0x0000000au); // unknown address
                write_register(0xb040, 0x0000000eu); // unknown address
                write_register(0xb0c8, 0x00000000u); // unknown address
                write_register(0xb040, 0x00000006u); // unknown address
                write_register(0xb040, 0x00000004u); // unknown address
                write_register(0x0000, 0x4f006442u); // unknown address
                write_register(0x0000, 0x0f006442u); // unknown address
                write_register(0x00b8, 0x00000401u); // unknown address
                write_register(0x00b8, 0x00000400u); // unknown address
                write_register(0xb07c, 0x00000000u); // unknown address
                // }

                // issd_evk3_imx636_init in hal_psee_plugins/include/devices/imx636/imx636_evk3_issd.h {
                write_register(0x001c, 0x00000001u); // unknown address
                write_register(reset_address, 0x00000001u);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                write_register(reset_address, 0x00000000u);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                write_register(mipi_control_address, 0x00000158u);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                write_register(0xb044, 0x00000000u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(300));
                write_register(0xb004, 0x0000000au); // unknown address
                write_register(0xb040, 0x00000000u); // unknown address
                write_register(0xb0c8, 0x00000000u); // unknown address
                write_register(0xb040, 0x00000000u); // unknown address
                write_register(0xb040, 0x00000000u); // unknown address
                write_register(0x0000, 0x4f006442u); // unknown address
                write_register(0x0000, 0x0f006442u); // unknown address
                write_register(0x00b8, 0x00000400u); // unknown address
                write_register(0x00b8, 0x00000400u); // unknown address
                write_register(0xb07c, 0x00000000u); // unknown address
                write_register(0xb074, 0x00000002u); // unknown address
                write_register(0xb078, 0x000000a0u); // unknown address
                write_register(0x00c0, 0x00000110u); // unknown address
                write_register(0x00c0, 0x00000210u); // unknown address
                write_register(0xb120, 0x00000001u); // unknown address
                write_register(0xe120, 0x00000000u); // unknown address
                write_register(0xb068, 0x00000004u); // unknown address
                write_register(0xb07c, 0x00000001u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                write_register(0xb07c, 0x00000003u); // unknown address
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                write_register(0x00b8, 0x00000401u); // unknown address
                write_register(0x00b8, 0x00000409u); // unknown address
                write_register(0x0000, 0x4f006442u); // unknown address
                write_register(0x0000, 0x4f00644au); // unknown address
                write_register(0xb080, 0x00000077u); // unknown address
                write_register(0xb084, 0x0000000fu); // unknown address
                write_register(0xb088, 0x00000037u); // unknown address
                write_register(0xb08c, 0x00000037u); // unknown address
                write_register(0xb090, 0x000000dfu); // unknown address
                write_register(0xb094, 0x00000057u); // unknown address
                write_register(0xb098, 0x00000037u); // unknown address
                write_register(0xb09c, 0x00000067u); // unknown address
                write_register(0xb0a0, 0x00000037u); // unknown address
                write_register(0xb0a4, 0x0000002fu); // unknown address
                write_register(0xb0ac, 0x00000028u); // unknown address
                write_register(0xb0cc, 0x00000001u); // unknown address
                write_register(mipi_control_address, 0x000002f8u);
                write_register(0xb004, 0x0000008au); // unknown address
                write_register(0xb01c, 0x00000030u); // unknown address
                write_register(mipi_packet_size_address, 0x00002000u);
                write_register(0xb02c, 0x000000ffu); // unknown address
                write_register(mipi_frame_blanking_address, 0x00003e80u);
                write_register(mipi_frame_period_address, 0x00000fa0u);
                write_register(0xa000, 0x000b0501u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa008, 0x00002405u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa004, 0x000b0501u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa020, 0x00000150u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xb040, 0x00000007u); // unknown address
                write_register(0xb064, 0x00000006u); // unknown address
                write_register(0xb040, 0x0000000fu); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                write_register(0xb004, 0x0000008au); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xb0c8, 0x00000003u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xb044, 0x00000001u); // unknown address
                write_register(mipi_control_address, 0x000002f9u);
                write_register(0x7008, 0x00000001u); // unknown address
                write_register(edf_pipeline_control_address, 0x00070001u);
                write_register(0x8000, 0x0001e085u); // unknown address
                write_register(time_base_ctrl_address, 0x00000644u);
                write_register(roi_ctrl_address, 0xf0005042u);
                write_register(spare0_address, 0x00000200u);
                write_register(bias_diff_address, 0x11a1504du);
                write_register(ro_fsm_ctrl_address, 0x00000000u);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                write_register(readout_ctrl_address, 0x00000200u);
                // }

                read_register(adc_control_address);
                write_register(adc_control_address, 0x00007641u);
                read_register(adc_control_address);
                write_register(adc_control_address, 0x00007643u);
                read_register(adc_misc_ctrl_address);
                write_register(adc_misc_ctrl_address, 0x00000212u);
                read_register(temp_ctrl_address);
                write_register(temp_ctrl_address, 0x00200082u);
                read_register(temp_ctrl_address);
                write_register(temp_ctrl_address, 0x00200083u);
                read_register(adc_control_address);
                write_register(adc_control_address, 0x00007641u);
                read_register(iph_mirr_ctrl_address);
                write_register(iph_mirr_ctrl_address, 0x00000003u);
                read_register(iph_mirr_ctrl_address);
                write_register(iph_mirr_ctrl_address, 0x00000003u);
                read_register(lifo_ctrl_address);
                write_register(lifo_ctrl_address, 0x00000001u);
                read_register(lifo_ctrl_address);
                write_register(lifo_ctrl_address, 0x00000003u);
                read_register(lifo_ctrl_address);
                write_register(lifo_ctrl_address, 0x00000007u);
                read_register(erc_reserved_6000_address);
                write_register(erc_reserved_6000_address, 0x00155400u);
                read_register(in_drop_rate_control_address);
                write_register(in_drop_rate_control_address, 0x00000001u);
                read_register(reference_period_address);
                write_register(reference_period_address, 0x000000c8u);
                read_register(td_target_event_rate_address);
                write_register(td_target_event_rate_address, 0x00000fa0u);
                read_register(erc_enable_address);
                write_register(erc_enable_address, 0x00000003u);

                // erc
                read_register(erc_reserved_602C_address);
                write_register(erc_reserved_602C_address, 0x00000001u);
                for (uint16_t address = erc_reserved_6800_6B98_begin; address < erc_reserved_6800_6B98_end;
                     address += 4) {
                    read_register(address);
                    write_register(address, 0x08080808u);
                }
                read_register(erc_reserved_602C_address);
                write_register(erc_reserved_602C_address, 0x00000002u);

                // t_drop_lut
                for (uint16_t address = t_drop_lut_begin; address < t_drop_lut_end; address += 4) {
                    read_register(address);
                    write_register(
                        address, (static_cast<uint32_t>(address / 2 + 1) << 16) | (static_cast<uint32_t>(address / 2)));
                }

                read_register(t_dropping_control_address);
                write_register(t_dropping_control_address, 0x00000000u);
                read_register(h_dropping_control_address);
                write_register(h_dropping_control_address, 0x00000000u);
                read_register(v_dropping_control_address);
                write_register(v_dropping_control_address, 0x00000000u);
                read_register(erc_reserved_6000_address);
                write_register(erc_reserved_6000_address, 0x00155401u);
                read_register(t_dropping_control_address);
                write_register(t_dropping_control_address, 0x00000000u);
                write_register(td_target_event_rate_address, 0x00000fa0u);

                // td_roi_x
                for (uint16_t address = td_roi_x_begin; address < td_roi_x_end; address += 4) {
                    const auto offset = (address - td_roi_x_begin) / 4;
                    if (offset % 2 == 0) {
                        write_register(
                            address, static_cast<uint32_t>(camera_parameters.x_mask[offset / 2] & 0xffffffffu));
                    } else {
                        write_register(
                            address,
                            static_cast<uint32_t>((camera_parameters.x_mask[offset / 2] & 0xffffffff00000000u) >> 32));
                    }
                }

                // td_roi_y
                for (uint16_t address = td_roi_y_begin; address < td_roi_y_end; address += 4) {
                    const auto offset = (address - td_roi_y_begin) / 4;
                    if (offset % 2 == 0) {
                        const auto bytes =
                            to_le_bytes(camera_parameters.y_mask[camera_parameters.y_mask.size() - 1 - (offset / 2)]);
                        const auto byte2 = std::get<0>(bytes);
                        const auto byte3 = std::get<1>(bytes);
                        if (offset < camera_parameters.y_mask.size() * 2 - 2) {
                            const auto bytes = to_le_bytes(
                                camera_parameters.y_mask[camera_parameters.y_mask.size() - 2 - (offset / 2)]);
                            const auto byte0 = std::get<6>(bytes);
                            const auto byte1 = std::get<7>(bytes);
                            write_register(
                                address,
                                from_le_bytes(
                                    reverse_bits(byte3),
                                    reverse_bits(byte2),
                                    reverse_bits(byte1),
                                    reverse_bits(byte0)));
                        } else {
                            write_register(
                                address, from_le_bytes(reverse_bits(byte3), reverse_bits(byte2), 0xff, 0x00));
                        }
                    } else {
                        const auto bytes =
                            to_le_bytes(camera_parameters.y_mask[camera_parameters.y_mask.size() - 2 - (offset / 2)]);
                        const auto byte0 = std::get<2>(bytes);
                        const auto byte1 = std::get<3>(bytes);
                        const auto byte2 = std::get<4>(bytes);
                        const auto byte3 = std::get<5>(bytes);
                        write_register(
                            address,
                            from_le_bytes(
                                reverse_bits(byte3), reverse_bits(byte2), reverse_bits(byte1), reverse_bits(byte0)));
                    }
                }
                write_register(
                    roi_ctrl_address, 0xf0005022 | (camera_parameters.mask_intersection_only ? 0 : (1 << 6)));

                read_register(edf_reserved_7004_address);
                write_register(edf_reserved_7004_address, 0x0000c5ffu);
                for (;;) {
                    std::vector<uint8_t> buffer(1 << 17);
                    _interface.bulk_transfer_accept_timeout("flushing the camera", 0x81, buffer, 100);
                    if (buffer.empty()) {
                        break;
                    }
                }
                bulk_request({0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 1000);
                send_parameters(camera_parameters, true);
                read_register(reference_period_address);
                read_register(td_target_event_rate_address);
                read_register(erc_reserved_6000_address);
                read_register(erc_reserved_6000_address);
                read_register(t_dropping_control_address);

                // issd_evk3_imx636_start in hal_psee_plugins/include/devices/imx636/imx636_evk3_issd.h {
                write_register(mipi_control_address, 0x000002f9u);
                write_register(ro_ctrl_address, 0x00000000u);
                read_register(time_base_ctrl_address);
                write_register(time_base_ctrl_address, 0x00000645u);
                write_register(0x002c, 0x0022c724u); // unknown address
                write_register(
                    roi_ctrl_address, 0xf0005422u | (camera_parameters.mask_intersection_only ? 0 : (1 << 6)));
                // }

                const auto bulk_timeout =
                    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());

                _loop = std::thread([this, bulk_timeout, buffers_count]() {
                    try {
                        std::vector<std::vector<uint8_t>> buffers(buffers_count);
                        std::vector<libusb_transfer*> transfers(buffers_count, nullptr);
                        for (auto& buffer : buffers) {
                            buffer.resize(1 << 17);
                        }
                        auto callback = [](libusb_transfer* transfer) {
                            auto that = reinterpret_cast<sepia::evk4::buffered_camera<HandleBuffer, HandleException>*>(
                                transfer->user_data);
                            try {
                                switch (transfer->status) {
                                    case LIBUSB_TRANSFER_CANCELLED: {
                                        if (that->_active_transfers > 0) {
                                            --that->_active_transfers;
                                        }
                                        break;
                                    }
                                    case LIBUSB_TRANSFER_COMPLETED:
                                    case LIBUSB_TRANSFER_TIMED_OUT:
                                    case LIBUSB_TRANSFER_STALL: {
                                        that->copy_and_push(
                                            transfer->buffer, transfer->buffer + transfer->actual_length);
                                        usb::throw_on_error(
                                            "libusb_(re)submit_transfer", libusb_submit_transfer(transfer));
                                        break;
                                    }
                                    case LIBUSB_TRANSFER_OVERFLOW: {
                                        throw std::runtime_error("LIBUSB_TRANSFER_OVERFLOW");
                                        break;
                                    }
                                    case LIBUSB_TRANSFER_ERROR: {
                                        throw std::runtime_error("LIBUSB_TRANSFER_ERROR");
                                        break;
                                    }
                                    case LIBUSB_TRANSFER_NO_DEVICE: {
                                        throw usb::device_disconnected(name);
                                        break;
                                    }
                                    default: {
                                        throw std::runtime_error(
                                            std::string("unknown transfer status ") + std::to_string(transfer->status));
                                        break;
                                    }
                                }
                            } catch (...) {
                                if (that->_active_transfers > 0) {
                                    --that->_active_transfers;
                                }
                                if (that->_running.exchange(false)) {
                                    that->_handle_exception(std::current_exception());
                                }
                            }
                        };
                        for (std::size_t index = 0; index < buffers_count; ++index) {
                            transfers[index] = libusb_alloc_transfer(0);
                            _interface.fill_bulk_transfer(
                                transfers[index],
                                (1 | LIBUSB_ENDPOINT_IN),
                                buffers[index],
                                callback,
                                this,
                                bulk_timeout);
                            transfers[index]->flags &= ~LIBUSB_TRANSFER_FREE_BUFFER;
                            transfers[index]->flags &= ~LIBUSB_TRANSFER_FREE_TRANSFER;
                            usb::throw_on_error(
                                std::string("libusb_submit_transfer ") + std::to_string(index),
                                libusb_submit_transfer(transfers[index]));
                        }
                        auto context = _interface.context();
                        timeval libusb_events_timeout;
                        libusb_events_timeout.tv_sec = 1;
                        libusb_events_timeout.tv_usec = 0;
                        int32_t completed = 0;
                        while (this->_running.load(std::memory_order_relaxed)) {
                            libusb_events_timeout.tv_sec = 1;
                            libusb_events_timeout.tv_usec = 0;
                            usb::throw_on_error(
                                "libusb_handle_events_completed",
                                libusb_handle_events_timeout_completed(
                                    context.get(), &libusb_events_timeout, &completed));
                        }
                        for (auto& transfer : transfers) {
                            libusb_cancel_transfer(transfer);
                        }
                        while (this->_active_transfers > 0) {
                            libusb_events_timeout.tv_sec = 0;
                            libusb_events_timeout.tv_usec = 10000;
                            usb::throw_on_error(
                                "libusb_handle_events_completed",
                                libusb_handle_events_timeout_completed(
                                    context.get(), &libusb_events_timeout, &completed));
                        }
                        for (auto& transfer : transfers) {
                            libusb_free_transfer(transfer);
                        }
                    } catch (...) {
                        if (this->_running.exchange(false)) {
                            this->_handle_exception(std::current_exception());
                        }
                    }
                });
                _parameters_loop = std::thread([this, timeout]() {
                    try {
                        while (this->_running.load(std::memory_order_relaxed)) {
                            parameters local_parameters;
                            {
                                std::unique_lock<std::mutex> lock(this->_mutex);
                                if (!this->_update_required) {
                                    if (!this->_condition_variable.wait_for(
                                            lock, timeout, [this] { return this->_update_required; })) {
                                        continue;
                                    }
                                }
                                local_parameters = this->_parameters;
                                this->_update_required = false;
                            }
                            send_parameters(local_parameters, false);
                        }
                    } catch (...) {
                        if (this->_running.exchange(false)) {
                            this->_handle_exception(std::current_exception());
                        }
                    }
                });
            }
            buffered_camera(const buffered_camera&) = delete;
            buffered_camera(buffered_camera&& other) = delete;
            buffered_camera& operator=(const buffered_camera&) = delete;
            buffered_camera& operator=(buffered_camera&& other) = delete;
            virtual ~buffered_camera() {
                this->_running.store(false, std::memory_order_relaxed);
                _loop.join();
                _parameters_loop.join();
                reset();
            }

            protected:
            /// bulk_request sends two bulk transfers (write then read).
            virtual std::vector<uint8_t> bulk_request(std::vector<uint8_t>&& bytes, uint32_t timeout) {
                _interface.bulk_transfer("bulk request", 0x02, bytes, timeout);
                bytes.resize(1024);
                return _interface.bulk_transfer("bulk response", 0x82, std::move(bytes), timeout);
            }

            /// write_register writes data to a 4-bytes register
            virtual std::vector<uint8_t> write_register(uint32_t address, uint32_t value) {
                return bulk_request(
                    {
                        0x02,
                        0x01,
                        0x01,
                        0x40,
                        0x0c,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        static_cast<uint8_t>(address & 0xff),
                        static_cast<uint8_t>((address >> 8) & 0xff),
                        static_cast<uint8_t>((address >> 16) & 0xff),
                        static_cast<uint8_t>((address >> 24) & 0xff),
                        static_cast<uint8_t>(value & 0xff),
                        static_cast<uint8_t>((value >> 8) & 0xff),
                        static_cast<uint8_t>((value >> 16) & 0xff),
                        static_cast<uint8_t>((value >> 24) & 0xff),
                    },
                    1000);
            }

            /// read_register reads data from a 4-bytes register
            virtual uint32_t read_register(uint32_t address) {
                const auto result = bulk_request(
                    {
                        0x02,
                        0x01,
                        0x01,
                        0x00,
                        0x0c,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        static_cast<uint8_t>(address & 0xff),
                        static_cast<uint8_t>((address >> 8) & 0xff),
                        static_cast<uint8_t>((address >> 16) & 0xff),
                        static_cast<uint8_t>((address >> 24) & 0xff),
                        0x01,
                        0x00,
                        0x00,
                        0x00,
                    },
                    1000);
                if (result.size() != 20) {
                    throw std::runtime_error(
                        std::string("read_register(") + std::to_string(address) + ") returned "
                        + std::to_string(result.size()) + " bytes (expected 20)");
                }
                const auto expected_bytes = std::vector<uint8_t>({
                    0x02,
                    0x01,
                    0x01,
                    0x00,
                    0x0c,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                    0x00,
                    static_cast<uint8_t>(address & 0xff),
                    static_cast<uint8_t>((address >> 8) & 0xff),
                    static_cast<uint8_t>((address >> 16) & 0xff),
                    static_cast<uint8_t>((address >> 24) & 0xff),
                });
                for (std::size_t index = 0; index < expected_bytes.size(); ++index) {
                    if (result[index] != expected_bytes[index]) {
                        throw std::runtime_error(
                            std::string("read_register(") + std::to_string(address) + ") returned unexpected data");
                    }
                }
                return static_cast<uint32_t>(result[16]) | (static_cast<uint32_t>(result[17]) << 8)
                       | (static_cast<uint32_t>(result[18]) << 16) | (static_cast<uint32_t>(result[19]) << 24);
            }

            /// reset sends destructor packets.
            virtual void reset() {
                // issd_evk3_imx636_stop in hal_psee_plugins/include/devices/imx636/imx636_evk3_issd.h {
                write_register(roi_ctrl_address, 0xf0005042u);
                write_register(0x002c, 0x0022c324u); // unknown address
                write_register(ro_ctrl_address, 0x00000002u);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                read_register(time_base_ctrl_address);
                write_register(time_base_ctrl_address, 0x00000644u);
                write_register(mipi_control_address, 0x000002f8u);
                std::this_thread::sleep_for(std::chrono::microseconds(300));
                // }

                // issd_evk3_imx636_destroy in hal_psee_plugins/include/devices/imx636/imx636_evk3_issd.h {
                write_register(0x0070, 0x00400008u); // unknown address
                write_register(0x006c, 0x0ee47114u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                write_register(0xa00c, 0x00020400u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                write_register(0xa010, 0x00008068u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0x1104, 0x00000000u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa020, 0x00000050u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa004, 0x000b0500u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa008, 0x00002404u); // unknown address
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                write_register(0xa000, 0x000b0500u); // unknown address
                write_register(0xb044, 0x00000000u); // unknown address
                write_register(0xb004, 0x0000000au); // unknown address
                write_register(0xb040, 0x0000000eu); // unknown address
                write_register(0xb0c8, 0x00000000u); // unknown address
                write_register(0xb040, 0x00000006u); // unknown address
                write_register(0xb040, 0x00000004u); // unknown address
                write_register(0x0000, 0x4f006442u); // unknown address
                write_register(0x0000, 0x0f006442u); // unknown address
                write_register(0x00b8, 0x00000401u); // unknown address
                write_register(0x00b8, 0x00000400u); // unknown address
                write_register(0xb07c, 0x00000000u); // unknown address
                // }
            }

            /// send_parameters updates the camera parameters.
            virtual void send_parameters(const parameters& camera_parameters, bool force) {
                sepia_evk4_bias(
                    bias_pr_address, pr, bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_fo_address, fo, bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_hpf_address, hpf, bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_diff_on_address,
                    diff_on,
                    bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_diff_address, diff, bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_diff_off_address,
                    diff_off,
                    bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_inv_address, inv, bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_refr_address, refr, bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_reqpuy_address,
                    reqpuy,
                    bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_reqpux_address,
                    reqpux,
                    bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_sendreqpdy_address,
                    sendreqpdy,
                    bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_unknown_1_address,
                    unknown_1,
                    bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                sepia_evk4_bias(
                    bias_unknown_2_address,
                    unknown_2,
                    bgen_buf_stg(1) | bgen_mux_en | bgen_buf_en | bgen_idac_en | bgen_single);
                _previous_parameters = camera_parameters;
            }

            /// read loads control data from the camera.
            virtual std::vector<uint8_t> read(uint8_t b_request, uint16_t w_value, uint16_t w_index, std::size_t size) {
                std::vector<uint8_t> buffer(size);
                _interface.control_transfer("read state", 0xc0, b_request, w_value, w_index, buffer);
                return buffer;
            }

            usb::interface _interface;
            std::thread _loop;
            std::thread _parameters_loop;
            parameters _previous_parameters;
            std::size_t _active_transfers;
        };

        /// decode implements a byte stream decoder for the PEK3SVCD camera.
        template <typename HandleEvent, typename HandleTriggerEvent, typename BeforeBuffer, typename AfterBuffer>
        class decode {
            public:
            decode(
                HandleEvent&& handle_event,
                HandleTriggerEvent&& handle_trigger_event,
                BeforeBuffer&& before_buffer,
                AfterBuffer&& after_buffer) :
                _handle_event(std::forward<HandleEvent>(handle_event)),
                _handle_trigger_event(std::forward<HandleTriggerEvent>(handle_trigger_event)),
                _before_buffer(std::forward<BeforeBuffer>(before_buffer)),
                _after_buffer(std::forward<AfterBuffer>(after_buffer)),
                _previous_msb_t(0),
                _previous_lsb_t(0),
                _overflows(0),
                _event({0, 0, 0, false}) {}
            decode(const decode&) = default;
            decode(decode&& other) = default;
            decode& operator=(const decode&) = default;
            decode& operator=(decode&& other) = default;
            virtual ~decode() {}

            /// operator() decodes a buffer of bytes.
            virtual void operator()(const std::vector<uint8_t>& buffer, std::size_t used, std::size_t size) {
                const auto dispatch = _before_buffer(used, size);
                const auto system_timestamp =
                    *reinterpret_cast<const uint64_t*>(buffer.data() + (buffer.size() - sizeof(uint64_t)));
                if (dispatch) {
                    for (std::size_t index = 0; index < ((buffer.size() - sizeof(uint64_t)) / 2) * 2; index += 2) {
                        switch (buffer[index + 1] >> 4) {
                            case 0b0000:
                                _event.y = static_cast<uint16_t>(
                                    buffer[index] | (static_cast<uint16_t>(buffer[index + 1] & 0b111) << 8));
                                if (_event.y < height) {
                                    _event.y = static_cast<uint16_t>(height - 1 - _event.y);
                                }
                                break;
                            case 0b0010:
                                _event.x = static_cast<uint16_t>(
                                    buffer[index] | (static_cast<uint16_t>(buffer[index + 1] & 0b111) << 8));
                                _event.on = ((buffer[index + 1] >> 3) & 1) == 1;
                                if (_event.x < width && _event.y < height) {
                                    _handle_event(_event);
                                }
                                break;
                            case 0b0011:
                                _event.x = static_cast<uint16_t>(
                                    buffer[index] | (static_cast<uint16_t>(buffer[index + 1] & 0b111) << 8));
                                _event.on = ((buffer[index + 1] >> 3) & 1) == 1;
                                break;
                            case 0b0100:
                                for (uint8_t bit = 0; bit < 8; ++bit) {
                                    if (((buffer[index] >> bit) & 1) == 1) {
                                        if (_event.x < width && _event.y < height) {
                                            _handle_event(_event);
                                        }
                                    }
                                    ++_event.x;
                                }
                                for (uint8_t bit = 0; bit < 4; ++bit) {
                                    if (((buffer[index + 1] >> bit) & 1) == 1) {
                                        if (_event.x < width && _event.y < height) {
                                            _handle_event(_event);
                                        }
                                    }
                                    ++_event.x;
                                }
                                break;
                            case 0b0101:
                                for (uint8_t bit = 0; bit < 8; ++bit) {
                                    if (((buffer[index] >> bit) & 1) == 1) {
                                        if (_event.x < width && _event.y < height) {
                                            _handle_event(_event);
                                        }
                                    }
                                    ++_event.x;
                                }
                                break;
                            case 0b0110: {
                                const auto lsb_t = static_cast<uint32_t>(
                                    buffer[index] | (static_cast<uint32_t>(buffer[index + 1] & 0b1111) << 8));
                                if (lsb_t != _previous_lsb_t) {
                                    _previous_lsb_t = lsb_t;
                                    const auto t = static_cast<uint64_t>(_previous_lsb_t | (_previous_msb_t << 12))
                                                   + (static_cast<uint64_t>(_overflows) << 24);
                                    if (t >= _event.t) {
                                        _event.t = t;
                                    }
                                }
                                break;
                            }
                            case 0b1000: {
                                const auto msb_t = static_cast<uint32_t>(
                                    buffer[index] | (static_cast<uint32_t>(buffer[index + 1] & 0b1111) << 8));
                                if (msb_t != _previous_msb_t) {
                                    if (msb_t > _previous_msb_t) {
                                        if (msb_t - _previous_msb_t < static_cast<uint32_t>((1 << 12) - 2)) {
                                            _previous_lsb_t = 0;
                                            _previous_msb_t = msb_t;
                                        }
                                    } else {
                                        if (_previous_msb_t - msb_t > static_cast<uint32_t>((1 << 12) - 2)) {
                                            ++_overflows;
                                            _previous_lsb_t = 0;
                                            _previous_msb_t = msb_t;
                                        }
                                    }
                                    const auto t = static_cast<uint64_t>(_previous_lsb_t | (_previous_msb_t << 12))
                                                   + (static_cast<uint64_t>(_overflows) << 24);
                                    if (t >= _event.t) {
                                        _event.t = t;
                                    }
                                }
                                break;
                            }
                            case 0b1010:
                                _handle_trigger_event(
                                    {_event.t,
                                     system_timestamp,
                                     static_cast<uint8_t>(buffer[index + 1] & 0b1111),
                                     (buffer[index] & 1) == 1});
                                break;
                            default:
                                break;
                        }
                    }
                } else {
                    for (std::size_t index = 0; index < ((buffer.size() - sizeof(uint64_t)) / 2) * 2; index += 2) {
                        switch (buffer[index + 1] >> 4) {
                            case 0b0000:
                                _event.y = static_cast<uint16_t>(
                                    buffer[index] | (static_cast<uint16_t>(buffer[index + 1] & 0b111) << 8));
                                if (_event.y < height) {
                                    _event.y = static_cast<uint16_t>(height - 1 - _event.y);
                                }
                                break;
                            case 0b0010:
                                _event.x = static_cast<uint16_t>(
                                    buffer[index] | (static_cast<uint16_t>(buffer[index + 1] & 0b111) << 8));
                                _event.on = ((buffer[index + 1] >> 3) & 1) == 1;
                                break;
                            case 0b0011:
                                _event.x = static_cast<uint16_t>(
                                    buffer[index] | (static_cast<uint16_t>(buffer[index + 1] & 0b111) << 8));
                                _event.on = ((buffer[index + 1] >> 3) & 1) == 1;
                                break;
                            case 0b0100:
                                _event.x += 12;
                                break;
                            case 0b0101:
                                _event.x += 8;
                                break;
                            case 0b0110: {
                                const auto lsb_t = static_cast<uint32_t>(
                                    buffer[index] | (static_cast<uint32_t>(buffer[index + 1] & 0b1111) << 8));
                                if (lsb_t != _previous_lsb_t) {
                                    _previous_lsb_t = lsb_t;
                                    const auto t = static_cast<uint64_t>(_previous_lsb_t | (_previous_msb_t << 12))
                                                   + (static_cast<uint64_t>(_overflows) << 24);
                                    if (t >= _event.t) {
                                        _event.t = t;
                                    }
                                }
                                break;
                            }
                            case 0b1000: {
                                const auto msb_t = static_cast<uint32_t>(
                                    buffer[index] | (static_cast<uint32_t>(buffer[index + 1] & 0b1111) << 8));
                                if (msb_t != _previous_msb_t) {
                                    if (msb_t > _previous_msb_t) {
                                        if (msb_t - _previous_msb_t < static_cast<uint32_t>((1 << 12) - 2)) {
                                            _previous_lsb_t = 0;
                                            _previous_msb_t = msb_t;
                                        }
                                    } else {
                                        if (_previous_msb_t - msb_t > static_cast<uint32_t>((1 << 12) - 2)) {
                                            ++_overflows;
                                            _previous_lsb_t = 0;
                                            _previous_msb_t = msb_t;
                                        }
                                    }
                                    const auto t = static_cast<uint64_t>(_previous_lsb_t | (_previous_msb_t << 12))
                                                   + (static_cast<uint64_t>(_overflows) << 24);
                                    if (t >= _event.t) {
                                        _event.t = t;
                                    }
                                }
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
                _after_buffer();
            }

            protected:
            HandleEvent _handle_event;
            HandleTriggerEvent _handle_trigger_event;
            BeforeBuffer _before_buffer;
            AfterBuffer _after_buffer;
            uint32_t _previous_msb_t;
            uint32_t _previous_lsb_t;
            uint32_t _overflows;
            sepia::dvs_event _event;
        };

        /// camera is an event observable connected to a CSD4MHDCD camera.
        template <
            typename HandleEvent,
            typename HandleTriggerEvent,
            typename BeforeBuffer,
            typename AfterBuffer,
            typename HandleException>
        class camera : public buffered_camera<
                           decode<HandleEvent, HandleTriggerEvent, BeforeBuffer, AfterBuffer>,
                           HandleException> {
            public:
            camera(
                HandleEvent&& handle_event,
                HandleTriggerEvent&& handle_trigger_event,
                BeforeBuffer&& before_buffer,
                AfterBuffer&& after_buffer,
                HandleException&& handle_exception,
                const parameters& camera_parameters = default_parameters,
                const std::string& serial = {},
                const std::chrono::steady_clock::duration& timeout = std::chrono::milliseconds(100),
                std::size_t buffers_count = 32,
                std::size_t fifo_size = 4096,
                std::function<void()> handle_drop = []() {}) :
                buffered_camera<decode<HandleEvent, HandleTriggerEvent, BeforeBuffer, AfterBuffer>, HandleException>(
                    decode<HandleEvent, HandleTriggerEvent, BeforeBuffer, AfterBuffer>(
                        std::forward<HandleEvent>(handle_event),
                        std::forward<HandleTriggerEvent>(handle_trigger_event),
                        std::forward<BeforeBuffer>(before_buffer),
                        std::forward<AfterBuffer>(after_buffer)),
                    std::forward<HandleException>(handle_exception),
                    camera_parameters,
                    serial,
                    timeout,
                    buffers_count,
                    fifo_size,
                    handle_drop) {}
            camera(const camera&) = delete;
            camera(camera&& other) = delete;
            camera& operator=(const camera&) = delete;
            camera& operator=(camera&& other) = delete;
            virtual ~camera() {}
        };

        /// make_camera creates a camera from functors.
        template <
            typename HandleEvent,
            typename HandleTriggerEvent,
            typename BeforeBuffer,
            typename AfterBuffer,
            typename HandleException>
        std::unique_ptr<camera<HandleEvent, HandleTriggerEvent, BeforeBuffer, AfterBuffer, HandleException>>
        make_camera(
            HandleEvent&& handle_event,
            HandleTriggerEvent&& handle_trigger_event,
            BeforeBuffer&& before_buffer,
            AfterBuffer&& after_buffer,
            HandleException&& handle_exception,
            const parameters& camera_parameters = default_parameters,
            const std::string& serial = {},
            const std::chrono::steady_clock::duration& timeout = std::chrono::milliseconds(100),
            std::size_t buffers_count = 32,
            std::size_t fifo_size = 4096,
            std::function<void()> handle_drop = []() {}) {
            return sepia::make_unique<
                camera<HandleEvent, HandleTriggerEvent, BeforeBuffer, AfterBuffer, HandleException>>(
                std::forward<HandleEvent>(handle_event),
                std::forward<HandleTriggerEvent>(handle_trigger_event),
                std::forward<BeforeBuffer>(before_buffer),
                std::forward<AfterBuffer>(after_buffer),
                std::forward<HandleException>(handle_exception),
                camera_parameters,
                serial,
                timeout,
                buffers_count,
                fifo_size,
                std::move(handle_drop));
        }
    }
}
