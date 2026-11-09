/**
 ****************************************************************************************
 *
 * @file user_peripheral.c
 *
 * @brief Peripheral project source code.
 *
 * Copyright (C) 2015-2023 Renesas Electronics Corporation and/or its affiliates.
 * All rights reserved. Confidential Information.
 *
 * This software ("Software") is supplied by Renesas Electronics Corporation and/or its
 * affiliates ("Renesas"). Renesas grants you a personal, non-exclusive, non-transferable,
 * revocable, non-sub-licensable right and license to use the Software, solely if used in
 * or together with Renesas products. You may make copies of this Software, provided this
 * copyright notice and disclaimer ("Notice") is included in all such copies. Renesas
 * reserves the right to change or discontinue the Software at any time without notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS". RENESAS DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. TO THE
 * MAXIMUM EXTENT PERMITTED UNDER LAW, IN NO EVENT SHALL RENESAS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE, EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES. USE OF THIS SOFTWARE MAY BE SUBJECT TO TERMS AND CONDITIONS CONTAINED IN
 * AN ADDITIONAL AGREEMENT BETWEEN YOU AND RENESAS. IN CASE OF CONFLICT BETWEEN THE TERMS
 * OF THIS NOTICE AND ANY SUCH ADDITIONAL LICENSE AGREEMENT, THE TERMS OF THE AGREEMENT
 * SHALL TAKE PRECEDENCE. BY CONTINUING TO USE THIS SOFTWARE, YOU AGREE TO THE TERMS OF
 * THIS NOTICE.IF YOU DO NOT AGREE TO THESE TERMS, YOU ARE NOT PERMITTED TO USE THIS
 * SOFTWARE.
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup APP
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"             // SW configuration
#include "gattc_task.h"
#include "gap.h"
#include "app_easy_timer.h"
#include "ble_general_sensor_service.h"
#include "user_custs1_impl.h"
#include "user_custs1_def.h"
#include "co_bt.h"
#include "app.h"
#if defined(CFG_PRINTF)
#include "arch_console.h"
#endif
#include "i2c.h"
#include "MCP9808.h"
#include "ADXL345.h"
#include "user_periph_setup.h"
#if defined(CFG_PRF_SUOTAR)
#include "app_suotar.h"
#include "custom_common.h"
#endif

extern const i2c_cfg_t i2c_cfg_ADXL345;
extern const i2c_cfg_t i2c_cfg_MCP9808;
extern const spi_cfg_t spi_cfg;
extern const spi_flash_cfg_t spi_flash_cfg;
extern diagnostic_char_t diagnostic_data;

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

// Manufacturer Specific Data ADV structure type
struct mnf_specific_data_ad_structure
{
    uint8_t ad_structure_size;
    uint8_t ad_structure_type;
    uint8_t company_id[APP_AD_MSD_COMPANY_ID_LEN];
    uint8_t proprietary_data[APP_AD_MSD_DATA_LEN];
};

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

uint8_t app_connection_idx                      __SECTION_ZERO("retention_mem_area0");
timer_hnd app_adv_data_update_timer_used        __SECTION_ZERO("retention_mem_area0");
timer_hnd app_param_update_request_timer_used   __SECTION_ZERO("retention_mem_area0");

// Retained variables
struct mnf_specific_data_ad_structure mnf_data  __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
// Index of manufacturer data in advertising data or scan response data (when MSB is 1)
uint8_t mnf_data_index                          __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_adv_data_len                     __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_scan_rsp_data_len                __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_adv_data[ADV_DATA_LEN]           __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_scan_rsp_data[SCAN_RSP_DATA_LEN] __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
extern timer_hnd adxl345_initiate_timer;      			  
extern timer_hnd mcp9808_initiate_timer;  
extern timer_hnd adxl345_capture_timer;      			  
extern timer_hnd mcp9808_capture_timer; 
extern timer_hnd rssi_timer;
uint16_t acc_cccd																__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t tmp_cccd																__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
*/

void reset_indication(uint16_t reset_stat)
{
    diagnostic_data.reset_cause = reset_stat;
}

/**
 ****************************************************************************************
 * @brief Initialize Manufacturer Specific Data
 ****************************************************************************************
 */
static void mnf_data_init()
{
    mnf_data.ad_structure_size = sizeof(struct mnf_specific_data_ad_structure ) - sizeof(uint8_t); // minus the size of the ad_structure_size field
    mnf_data.ad_structure_type = GAP_AD_TYPE_MANU_SPECIFIC_DATA;
    mnf_data.company_id[0] = APP_AD_MSD_COMPANY_ID & 0xFF; // LSB
    mnf_data.company_id[1] = (APP_AD_MSD_COMPANY_ID >> 8 )& 0xFF; // MSB
    mnf_data.proprietary_data[0] = 0;
    mnf_data.proprietary_data[1] = 0;
}

/**
 ****************************************************************************************
 * @brief Update Manufacturer Specific Data
 ****************************************************************************************
 */
static void mnf_data_update()
{
    uint16_t data;

    data = mnf_data.proprietary_data[0] | (mnf_data.proprietary_data[1] << 8);
    data += 1;
    mnf_data.proprietary_data[0] = data & 0xFF;
    mnf_data.proprietary_data[1] = (data >> 8) & 0xFF;

    if (data == 0xFFFF) {
         mnf_data.proprietary_data[0] = 0;
         mnf_data.proprietary_data[1] = 0;
    }
}

/**
 ****************************************************************************************
 * @brief Add an AD structure in the Advertising or Scan Response Data of the
 *        GAPM_START_ADVERTISE_CMD parameter struct.
 * @param[in] cmd               GAPM_START_ADVERTISE_CMD parameter struct
 * @param[in] ad_struct_data    AD structure buffer
 * @param[in] ad_struct_len     AD structure length
 * @param[in] adv_connectable   Connectable advertising event or not. It controls whether
 *                              the advertising data use the full 31 bytes length or only
 *                              28 bytes (Document CCSv6 - Part 1.3 Flags).
 ****************************************************************************************
 */
static void app_add_ad_struct(struct gapm_start_advertise_cmd *cmd, void *ad_struct_data, uint8_t ad_struct_len, uint8_t adv_connectable)
{
    uint8_t adv_data_max_size = (adv_connectable) ? (ADV_DATA_LEN - 3) : (ADV_DATA_LEN);

    if ((adv_data_max_size - cmd->info.host.adv_data_len) >= ad_struct_len)
    {
        // Append manufacturer data to advertising data
        memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len], ad_struct_data, ad_struct_len);

        // Update Advertising Data Length
        cmd->info.host.adv_data_len += ad_struct_len;

        // Store index of manufacturer data which are included in the advertising data
        mnf_data_index = cmd->info.host.adv_data_len - sizeof(struct mnf_specific_data_ad_structure);
    }
    else if ((SCAN_RSP_DATA_LEN - cmd->info.host.scan_rsp_data_len) >= ad_struct_len)
    {
        // Append manufacturer data to scan response data
        memcpy(&cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len], ad_struct_data, ad_struct_len);

        // Update Scan Response Data Length
        cmd->info.host.scan_rsp_data_len += ad_struct_len;

        // Store index of manufacturer data which are included in the scan response data
        mnf_data_index = cmd->info.host.scan_rsp_data_len - sizeof(struct mnf_specific_data_ad_structure);
        // Mark that manufacturer data is in scan response and not advertising data
        mnf_data_index |= 0x80;
    }
    else
    {
        // Manufacturer Specific Data do not fit in either Advertising Data or Scan Response Data
        ASSERT_WARNING(0);
    }
    // Store advertising data length
    stored_adv_data_len = cmd->info.host.adv_data_len;
    // Store advertising data
    memcpy(stored_adv_data, cmd->info.host.adv_data, stored_adv_data_len);
    // Store scan response data length
    stored_scan_rsp_data_len = cmd->info.host.scan_rsp_data_len;
    // Store scan_response data
    memcpy(stored_scan_rsp_data, cmd->info.host.scan_rsp_data, stored_scan_rsp_data_len);
}

/**
 ****************************************************************************************
 * @brief Advertisement data update timer callback function.
 ****************************************************************************************
*/
static void adv_data_update_timer_cb()
{
    // If mnd_data_index has MSB set, manufacturer data is stored in scan response
    uint8_t *mnf_data_storage = (mnf_data_index & 0x80) ? stored_scan_rsp_data : stored_adv_data;

    // Update manufacturer data
    mnf_data_update();

    // Update the selected fields of the advertising data (manufacturer data)
    memcpy(mnf_data_storage + (mnf_data_index & 0x7F), &mnf_data, sizeof(struct mnf_specific_data_ad_structure));

    // Update advertising data on the fly
    app_easy_gap_update_adv_data(stored_adv_data, stored_adv_data_len, stored_scan_rsp_data, stored_scan_rsp_data_len);

    // Restart timer for the next advertising update
    app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);
}

/**
 ****************************************************************************************
 * @brief Parameter update request timer callback function.
 ****************************************************************************************
*/
static void param_update_request_timer_cb()
{
    app_easy_gap_param_update_start(app_connection_idx);
    app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
}

void user_app_update_params_rejected(const uint8_t status)
{
#if defined(CFG_PRINTF)
    arch_printf("Conn param update REJECTED by master, status 0x%02X\r\n", status);
#endif
}

void user_app_update_params_complete(void)
{
#if defined(CFG_PRINTF)
    arch_puts("Conn param update request accepted\r\n");
#endif
}

bool i2c_wait_idle(void)
{
    for (uint8_t i = 0; i < I2C_IDLE_POLL_COUNT; i++)
    {
        if (!i2c_is_master_busy())
        {
            return true;
        }
        arch_asm_delay_us(I2C_IDLE_POLL_US);
    }
		
    return false;
}

#if defined(CFG_SPI_FLASH_ENABLE)
static void boot_flash_init_power_down(void)
{
    uint8_t dev_id;
	
		// Disable HW Reset functionality of P0_0
		GPIO_Disable_HW_Reset();
	
		// Initialize interface to SPI flash so we can use it from within the application
		spi_flash_configure_env(&spi_flash_cfg);
		spi_initialize(&spi_cfg);
	
    // Release the SPI flash memory from power down
    spi_flash_release_from_power_down();

    // Try to auto-detect the SPI flash memory
    spi_flash_auto_detect(&dev_id);

    // Power down flash
    spi_flash_power_down();
}
#endif // CFG_SPI_FLASH_ENABLE

#if defined(CFG_PRF_SUOTAR)
static void update_parameters_for_SUOTA()
{
		struct gapc_param_update_cmd* cmd = KE_MSG_ALLOC(GAPC_PARAM_UPDATE_CMD,
                                                     KE_BUILD_ID(TASK_GAPC, app_connection_idx),
                                                     TASK_APP,
                                                     gapc_param_update_cmd);

    cmd->operation = GAPC_UPDATE_PARAMS;

		cmd->intv_max = MS_TO_DOUBLESLOTS(10);
		cmd->intv_min = MS_TO_DOUBLESLOTS(10);
		cmd->latency = 0;
		cmd->time_out = MS_TO_TIMERUNITS(1250);
		cmd->ce_len_min = MS_TO_DOUBLESLOTS(0);
		cmd->ce_len_max = MS_TO_DOUBLESLOTS(0);

		// Send the message
    KE_MSG_SEND(cmd);
}

static void restore_parameters_after_SUOTA()
{
		struct gapc_param_update_cmd* cmd = KE_MSG_ALLOC(GAPC_PARAM_UPDATE_CMD,
                                                     KE_BUILD_ID(TASK_GAPC, app_connection_idx),
                                                     TASK_APP,
                                                     gapc_param_update_cmd);

    cmd->operation = GAPC_UPDATE_PARAMS;

		cmd->intv_max = MS_TO_DOUBLESLOTS(200);
		cmd->intv_min = MS_TO_DOUBLESLOTS(250);
		cmd->latency = 4;
		cmd->time_out = MS_TO_TIMERUNITS(4000);
		cmd->ce_len_min = MS_TO_DOUBLESLOTS(0);
		cmd->ce_len_max = MS_TO_DOUBLESLOTS(0);

		// Send the message
    KE_MSG_SEND(cmd);
}

void on_suotar_status_change(const uint8_t suotar_event)
{
    uint8_t dev_id;
	
		// Disable HW Reset functionality of P0_0
		GPIO_Disable_HW_Reset();
	
#if defined(CFG_SPI_FLASH_ENABLE)
		// Initialize interface to SPI flash so we can use it from within the application
		spi_flash_configure_env(&spi_flash_cfg);
		spi_initialize(&spi_cfg);
#endif

    // Release Flash from power down
    spi_flash_release_from_power_down();

    // Try to auto-detect the device
    spi_flash_auto_detect(&dev_id);

		if (suotar_event == SUOTAR_START)
		{
				if (app_param_update_request_timer_used != EASY_TIMER_INVALID_TIMER)
				{
						app_easy_timer_cancel(app_param_update_request_timer_used);
						app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
				}

				update_parameters_for_SUOTA();
		}
    else if (suotar_event == SUOTAR_END)
    {
        // Power down SPI Flash
        spi_flash_power_down();
			
				restore_parameters_after_SUOTA();
    }
}
#endif

void user_app_init(void)
{
    app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;

#if defined(CFG_SPI_FLASH_ENABLE)
		// Make sure the flash is put to sleep when booting
    boot_flash_init_power_down();
#endif

    // Initialize Manufacturer Specific Data
    mnf_data_init();

    // Initialize Advertising and Scan Response Data
    memcpy(stored_adv_data, USER_ADVERTISE_DATA, USER_ADVERTISE_DATA_LEN);
    stored_adv_data_len = USER_ADVERTISE_DATA_LEN;
    memcpy(stored_scan_rsp_data, USER_ADVERTISE_SCAN_RESPONSE_DATA, USER_ADVERTISE_SCAN_RESPONSE_DATA_LEN);
    stored_scan_rsp_data_len = USER_ADVERTISE_SCAN_RESPONSE_DATA_LEN;
	
		power_down_adxl345();
		power_down_mcp9808();
	
		diagnostic_data.format_version = DIAG_FORMAT_VERSION;
	
    default_app_on_init();
#if defined(CFG_PRINTF)
		arch_puts("App Initialized\r\n");
#endif
}

void user_app_adv_start(void)
{
    // Schedule the next advertising data update
    app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);

    struct gapm_start_advertise_cmd* cmd;
    cmd = app_easy_gap_undirected_advertise_get_active();

    // Add manufacturer data to initial advertising or scan response data, if there is enough space
    app_add_ad_struct(cmd, &mnf_data, sizeof(struct mnf_specific_data_ad_structure), 1);

    app_easy_gap_undirected_advertise_start();
	
#if defined(CFG_PRINTF)
		arch_puts("Advertising Started\r\n");
#endif
}

void user_app_connection(uint8_t connection_idx, struct gapc_connection_req_ind const *param)
{
    if (app_env[connection_idx].conidx != GAP_INVALID_CONIDX)
    {
        app_connection_idx = connection_idx;

        // Stop the advertising data update timer
        app_easy_timer_cancel(app_adv_data_update_timer_used);

        // Check if the parameters of the established connection are the preferred ones.
        // If not then schedule a connection parameter update request.
        if ((param->con_interval < user_connection_param_conf.intv_min) ||
            (param->con_interval > user_connection_param_conf.intv_max) ||
            (param->con_latency != user_connection_param_conf.latency) ||
            (param->sup_to != user_connection_param_conf.time_out))
        {
            // Connection params are not these that we expect
            app_param_update_request_timer_used = app_easy_timer(APP_PARAM_UPDATE_REQUEST_TO, param_update_request_timer_cb);

#if defined(CFG_PRINTF)
						arch_puts("Connection Parameters update initiated\r\n");
#endif
        }

#if defined(CFG_PRINTF)
				arch_printf("App Connected: Index = %u\r\n", connection_idx);
#endif
				rssi_timer = app_easy_timer(RSSI_READ_PERIOD, request_connected_rssi);
    }
    else
    {
        // No connection has been established, restart advertising
        user_app_adv_start();
    }

    default_app_on_connection(connection_idx, param);
}

void user_app_adv_undirect_complete(uint8_t status)
{
#if defined(CFG_PRINTF)
		arch_printf("Advertising Ended: %u\r\n", status);
#endif
    // If advertising was canceled then update advertising data and start advertising again
    if (status == GAP_ERR_CANCELED)
    {
        user_app_adv_start();
    }
}

void user_app_disconnect(struct gapc_disconnect_ind const *param)
{
#if defined(CFG_PRINTF)
		arch_printf("App Disconnected: Index -> %lu, Reason: -> %u\r\n", param->conhdl, param->reason);
#endif
		diagnostic_data.disc_reason = param->reason;
	
		// Clear CCCD descriptor values upon disconnection
		acc_cccd = 0;
		tmp_cccd = 0;
	
    // Cancel the parameter update request timer
    if (app_param_update_request_timer_used != EASY_TIMER_INVALID_TIMER)
    {
        app_easy_timer_cancel(app_param_update_request_timer_used);
        app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
    }
    // Update manufacturer data for the next advertsing event
    mnf_data_update();
		
		// Cancel initiation and capture timers if they are still active after a sudden disconnection
		if (adxl345_initiate_timer != EASY_TIMER_INVALID_TIMER)
		{
#if defined(CFG_PRINTF)
				arch_puts("App Disconnected Unexpectedly ... disabling ADXL345 capture initiation timer callback\r\n");
#endif
				app_easy_timer_cancel(adxl345_initiate_timer);
				adxl345_initiate_timer = EASY_TIMER_INVALID_TIMER;
		}
		
		if (mcp9808_initiate_timer != EASY_TIMER_INVALID_TIMER)
		{
#if defined(CFG_PRINTF)
				arch_puts("App Disconnected Unexpectedly ... disabling MCP9808 capture initiation timer callback\r\n");
#endif
				app_easy_timer_cancel(mcp9808_initiate_timer);
				mcp9808_initiate_timer = EASY_TIMER_INVALID_TIMER;
		}
		
		if (adxl345_capture_timer != EASY_TIMER_INVALID_TIMER)
		{
#if defined(CFG_PRINTF)
				arch_puts("App Disconnected Unexpectedly ... disabling ADXL345 capture timer callback\r\n");
#endif
				app_easy_timer_cancel(adxl345_capture_timer);
				adxl345_capture_timer = EASY_TIMER_INVALID_TIMER;
		}
		
		if (mcp9808_capture_timer != EASY_TIMER_INVALID_TIMER)
		{
#if defined(CFG_PRINTF)
				arch_puts("App Disconnected Unexpectedly ... disabling MCP9808 capture timer callback\r\n");
#endif
				app_easy_timer_cancel(mcp9808_capture_timer);
				mcp9808_capture_timer = EASY_TIMER_INVALID_TIMER;
		}
		
		// Cancel the RSSI request timer
		if (rssi_timer != EASY_TIMER_INVALID_TIMER)
		{
#if defined(CFG_PRINTF)			
				arch_puts("App Disconnected Unexpectedly ... disabling RSSI request timer\r\n");
#endif				
				app_easy_timer_cancel(rssi_timer);
				rssi_timer = EASY_TIMER_INVALID_TIMER;
		}
		
		// Power down sensors in case of sudden disconnect where the normal program flow disrupts
		power_down_adxl345();
		power_down_mcp9808();
		
#if defined(CFG_PRF_SUOTAR)
		// Issue a platform reset when it is requested by the suotar procedure
		if (suota_state.reboot_requested)
		{
				// Reboot request will be served
				suota_state.reboot_requested = 0;

				// Platform reset
				platform_reset(RESET_AFTER_SUOTA_UPDATE);
		}
#endif
		
    // Restart Advertising
    user_app_adv_start();
}

#include "user_version.h"
void user_app_on_db_init_complete(void)
{
    /* Set the correct application version number in DISS 
			 The default value is the SDK_VERSION, same as the FIRMWARE_VERSION
		**/
    char sw_version[] = APP_VERSION;
	
    struct diss_set_value_req *req = KE_MSG_ALLOC_DYN(DISS_SET_VALUE_REQ,
                                                      prf_get_task_from_id(TASK_ID_DISS),
                                                      TASK_APP,
                                                      diss_set_value_req,
                                                      sizeof(sw_version));
    req->value = DIS_SW_REV_STR_CHAR;
    req->length = sizeof(sw_version) - 1;
    memcpy(req->data, sw_version, sizeof(sw_version) - 1);
		
    ke_msg_send(req);
		
#if defined(CFG_PRINTF)
    arch_puts("--- CUSTS1 handle map ---\r\n");
    for (uint8_t i = 0; i < CUSTS1_IDX_NB; i++)
    {
        arch_printf("att_idx %u -> handle %u\r\n", i, custs1_get_att_handle(i));
    }
    arch_puts("-------------------------\r\n");
#endif

    default_app_on_db_init_complete();
}


void user_catch_rest_hndl(ke_msg_id_t const msgid,
                          void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    switch(msgid)
    {
        case CUSTS1_VAL_WRITE_IND:
        {
            struct custs1_val_write_ind const *msg_param = (struct custs1_val_write_ind const *)(param);

            switch (msg_param->handle)
            {
                case SVC1_IDX_CONTROL_POINT_1_VAL:
                    user_svc1_ctrl_wr_ind_handler(msgid, msg_param, dest_id, src_id);
                    break;
								
								case SVC2_IDX_CONTROL_POINT_2_VAL:
                    user_svc2_ctrl_wr_ind_handler(msgid, msg_param, dest_id, src_id);
                    break;
								
								case SVC1_IDX_ACCELEROMETER_NTF_CFG:
								{
										// Check the length of the written value in CCCD of the Accelerometer
										if (msg_param->length != sizeof(uint16_t))
										{
#if defined(CFG_PRINTF)											
												arch_printf("Accelerometer CCC: bad length %u\r\n", msg_param->length);
#endif												
												break;
										}
										
										// Read the CCCD value for the Accelerometer
										acc_cccd = co_read16p(&msg_param->value[0]);
										
										diagnostic_data.acc_ccc = acc_cccd;
										
										if (acc_cccd == PRF_CLI_START_NTF)              /* 0x0001 */
										{
#if defined(CFG_PRINTF)											
												arch_puts("Accelerometer notifications enabled\r\n");
#endif										
										}
										else if (acc_cccd == PRF_CLI_STOP_NTFIND)       /* 0x0000 */
										{
#if defined(CFG_PRINTF)											
												arch_puts("Accelerometer notifications disabled\r\n");
#endif										
										}
										else
										{
#if defined(CFG_PRINTF)											
												arch_printf("Accelerometer CCC: unsupported value 0x%04X on a notify-only characteristic\r\n", acc_cccd);
#endif										
										}
								}break;
								
								case SVC2_IDX_TEMPERATURE_NTF_CFG:
								{
										// Check the length of the written value in CCCD of the Temperature
										if (msg_param->length != sizeof(uint16_t))
										{
#if defined(CFG_PRINTF)											
												arch_printf("Temperature CCC: bad length %u\r\n", msg_param->length);
#endif												
												break;
										}
										
										// Read the CCCD value for the Temperature
										tmp_cccd = co_read16p(&msg_param->value[0]);
										
										diagnostic_data.tmp_acc = tmp_cccd;
										
										if (tmp_cccd == PRF_CLI_START_NTF)              /* 0x0001 */
										{
#if defined(CFG_PRINTF)											
												arch_puts("Temperature notifications enabled\r\n");
#endif										
										}
										else if (tmp_cccd == PRF_CLI_STOP_NTFIND)       /* 0x0000 */
										{
#if defined(CFG_PRINTF)											
												arch_puts("Temperature notifications disabled\r\n");
#endif										
										}
										else
										{
#if defined(CFG_PRINTF)											
												arch_printf("Temperature CCC: unsupported value 0x%04X on a notify-only characteristic\r\n", tmp_cccd);
#endif										
										}
								}break;

                default:
                    break;
            }
        } break;

				// Do nothing when Notifications leave the Queue where they await for sending
        case CUSTS1_VAL_NTF_CFM:
        {
            struct custs1_val_ntf_cfm const *msg_param = (struct custs1_val_ntf_cfm const *)(param);

            switch (msg_param->handle)
            {
								case SVC1_IDX_ACCELEROMETER_VAL:
										break;
								
								case SVC2_IDX_TEMPERATURE_VAL:
										break;
								
                default:
                    break;
            }
        } break;

				// Indication messages don't exist in this example so they remain unhandled
        case CUSTS1_VAL_IND_CFM:
        {
            struct custs1_val_ind_cfm const *msg_param = (struct custs1_val_ind_cfm const *)(param);

            switch (msg_param->handle)
            {
                default:
                    break;
             }
        } break;

        case GAPC_PARAM_UPDATED_IND:
        {
            // Cast the "param" pointer to the appropriate message structure
            struct gapc_param_updated_ind const *msg_param = (struct gapc_param_updated_ind const *)(param);
	
#if defined(CFG_PRINTF)					
						// Connection Interval in 1.25ms Units, Supervision Timeout in 10 ms Units
						arch_printf("Connection Parameters: Interval ->  %u.%02ums, Latency -> %u, Timeout -> %u | ", 
																		(msg_param->con_interval) * 125 / 100, 
																		((msg_param->con_interval) * 125) % 100, 
																		msg_param->con_latency, 
																		msg_param->sup_to * 10);
#endif
            // Check if updated Conn Params filled to preferred ones
            if ((msg_param->con_interval >= user_connection_param_conf.intv_min) &&
                (msg_param->con_interval <= user_connection_param_conf.intv_max) &&
                (msg_param->con_latency == user_connection_param_conf.latency) &&
                (msg_param->sup_to == user_connection_param_conf.time_out))
            {
#if defined(CFG_PRINTF)							
								arch_puts("  -> WITHIN preferred range\r\n");
#endif            
						}
						else{
#if defined(CFG_PRINTF)							
								arch_puts("  -> OUTSIDE preferred range\r\n");
#endif						
						}
        } break;
				
				case GAPC_CON_RSSI_IND:
				{
						/* Print the calculated RSSI */
						struct gapc_con_rssi_ind const *msg_param = (struct gapc_con_rssi_ind const *)(param);
					
						diagnostic_data.last_rssi = (int8_t)(msg_param->rssi);
#if defined(CFG_PRINTF)					
						arch_printf("Connection -> %u, RSSI -> %d dBm\r\n", app_connection_idx, (int8_t)(msg_param->rssi));
#endif				
				} break;

				case GATTC_MTU_CHANGED_IND:
				{
						/* Print the newly negotiated MTU */
						struct gattc_mtu_changed_ind const *msg_param = (struct gattc_mtu_changed_ind const *)(param);
#if defined(CFG_PRINTF)					
						arch_printf("Connection -> %u, MTU negotiated: %u bytes\r\n", app_connection_idx, msg_param->mtu);
#endif				
				} break;

				case GAPC_CMP_EVT:
				{
						struct gapc_cmp_evt const *msg_param = (struct gapc_cmp_evt const *)(param);
					
						if (msg_param->status != GAP_ERR_NO_ERROR)
						{
#if defined(CFG_PRINTF)							
								arch_printf("GAPC op 0x%02X failed, status 0x%02X\r\n", msg_param->operation, msg_param->status);
#endif						
						}
				} break;

				case GATTC_CMP_EVT:
				{
						struct gattc_cmp_evt const *msg_param = (struct gattc_cmp_evt const *)(param);
					
						if (msg_param->status != GAP_ERR_NO_ERROR)
						{
#if defined(CFG_PRINTF)							
								arch_printf("GATTC op 0x%02X failed, status 0x%02X\r\n", msg_param->operation, msg_param->status);
#endif						
						}
				} break;
				
				case GAPM_CMP_EVT:
				{
						struct gapm_cmp_evt const *msg_param = (struct gapm_cmp_evt const *)(param);
					
						if (msg_param->status != GAP_ERR_NO_ERROR)
						{
#if defined(CFG_PRINTF)							
								arch_printf("GAPM op 0x%02X failed, status 0x%02X\r\n", msg_param->operation, msg_param->status);
#endif						
						}
				} break;
				
				case CUSTS1_VALUE_REQ_IND:
        {
            struct custs1_value_req_ind const *msg_param = (struct custs1_value_req_ind const *) param;
          
						switch (msg_param->att_idx)
            {
                case SVC3_IDX_DIAGNOSTIC_DATA_VAL:
                {
										diagnostic_char_ind_handler(msgid, msg_param, dest_id, src_id);
                } break;

                default:
                {
                    // Send Error message
                    struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC(CUSTS1_VALUE_REQ_RSP,
                                                                    src_id,
                                                                    dest_id,
                                                                    custs1_value_req_rsp);

                    // Provide the connection index.
                    rsp->conidx  = app_env[msg_param->conidx].conidx;
                    // Provide the attribute index.
                    rsp->att_idx = msg_param->att_idx;
                    // Force current length to zero.
                    rsp->length = 0;
                    // Set Error status
                    rsp->status  = ATT_ERR_APP_ERROR;
                    // Send message
                    KE_MSG_SEND(rsp);
                } break;
             }
        } break;

        case GATTC_EVENT_REQ_IND:
        {
            // Confirm unhandled indication to avoid GATT timeout
            struct gattc_event_ind const *ind = (struct gattc_event_ind const *) param;
            struct gattc_event_cfm *cfm = KE_MSG_ALLOC(GATTC_EVENT_CFM, src_id, dest_id, gattc_event_cfm);
            cfm->handle = ind->handle;
            KE_MSG_SEND(cfm);
        } break;

        default:
            break;
    }
}

/// @} APP
