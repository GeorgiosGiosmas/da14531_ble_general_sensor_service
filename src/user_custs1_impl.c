/**
 ****************************************************************************************
 *
 * @file user_custs1_impl.c
 *
 * @brief Peripheral project Custom1 Server implementation source code.
 *
 * Copyright (C) 2015-2025 Renesas Electronics Corporation and/or its affiliates.
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

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include "gpio.h"
#include "app_api.h"
#include "app.h"
#include "prf_utils.h"
#include "custs1.h"
#include "custs1_task.h"
#include "user_custs1_def.h"
#include "user_custs1_impl.h"
#include "ble_general_sensor_service.h" 
#include "user_periph_setup.h"
#include "ADXL345.h"
#include "MCP9808.h"
#include "arch_console.h"
#include "i2c.h"

extern uint8_t app_connection_idx    __SECTION_ZERO("retention_mem_area0");

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

timer_hnd adxl345_initiate_timer   												  __SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
timer_hnd mcp9808_initiate_timer   												  __SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
timer_hnd adxl345_capture_timer    												  __SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
timer_hnd mcp9808_capture_timer      												__SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
timer_hnd rssi_timer																				__SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
int16_t x																										__SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
int16_t y																										__SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
int16_t z																										__SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
int16_t temp_tenths																					__SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
uint8_t xyz[DEF_SVC1_ACC_DATA_CHAR_LEN]											__SECTION_ZERO("retention_mem_area0"); 				 //@RETENTION MEMORY
diagnostic_char_t diagnostic_data														__SECTION_ZERO("retention_mem_area0") = { 0 }; //@RETENTION MEMORY
extern uint16_t acc_cccd;																		
extern uint16_t tmp_cccd;																		

extern const i2c_cfg_t i2c_cfg_MCP9808;
extern const i2c_cfg_t i2c_cfg_ADXL345;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
 
void power_down_adxl345(void)
{
		i2c_init(&i2c_cfg_ADXL345);
		ADXL345_deinit();
	
		// Wait until no master activity and if the bus is stuck reset it
		if (!i2c_wait_idle())
		{
				arch_puts("ADXL345 power-down: bus stuck, recovering\r\n");
				i2c_release();               /* release the controller BEFORE bit-banging */
				i2c_bus_recovery();
				diagnostic_data.i2c_rec_cnt++;
		}
		else
		{
			i2c_release();
		}
}

void power_down_mcp9808(void)
{
		i2c_init(&i2c_cfg_MCP9808);
		MCP9808_deinit();
	
		// Wait until no master activity and if the bus is stuck reset it
		if (!i2c_wait_idle())
		{
				arch_puts("MCP9808 power-down: bus stuck, recovering\r\n");
				i2c_release();               /* release the controller BEFORE bit-banging */
				i2c_bus_recovery();
				diagnostic_data.i2c_rec_cnt++;
		}
		else
		{
			i2c_release();
		}
}

void user_svc1_ctrl_wr_ind_handler(ke_msg_id_t const msgid,
                                      struct custs1_val_write_ind const *param,
                                      ke_task_id_t const dest_id,
                                      ke_task_id_t const src_id)
{
    uint8_t val = 0;
    memcpy(&val, &param->value[0], param->length);

    if (val == ENABLE_SENSOR_DATA_CAPTURING)
    {
				if (adxl345_initiate_timer == EASY_TIMER_INVALID_TIMER && adxl345_capture_timer == EASY_TIMER_INVALID_TIMER)
				{
						arch_puts("Control Point 1 Activated\r\n");
						adxl345_initiate_timer = app_easy_timer(I2C_DATA_CAPTURE_INITIATION_PERIOD_ADXL345, initiate_adxl345_for_data_capture);
				}
				else
				{
						arch_puts("ADXL345 capture already running\r\n");
				}
    }
    else if(val == DISABLE_SENSOR_DATA_CAPTURING)
    {
        if (adxl345_initiate_timer != EASY_TIMER_INVALID_TIMER)
        {
						arch_puts("Control Point 1 Deactivated -> Stopping ADXL345 capture initiation\r\n");
            app_easy_timer_cancel(adxl345_initiate_timer);
            adxl345_initiate_timer = EASY_TIMER_INVALID_TIMER;
        }
				else if (adxl345_capture_timer != EASY_TIMER_INVALID_TIMER)
        {
						arch_puts("Control Point 1 Deactivated -> Stopping ADXL345 capture and notif\r\n");
            app_easy_timer_cancel(adxl345_capture_timer);
            adxl345_capture_timer = EASY_TIMER_INVALID_TIMER;
        }
				
				power_down_adxl345();
    }
		else
				arch_puts("Invalid Value...Try 0x00 or 0x01\r\n");
}

void user_svc2_ctrl_wr_ind_handler(ke_msg_id_t const msgid,
                                      struct custs1_val_write_ind const *param,
                                      ke_task_id_t const dest_id,
                                      ke_task_id_t const src_id)
{
    uint8_t val = 0;
    memcpy(&val, &param->value[0], param->length);

    if (val == ENABLE_SENSOR_DATA_CAPTURING)
    {
				if (mcp9808_initiate_timer == EASY_TIMER_INVALID_TIMER && mcp9808_capture_timer == EASY_TIMER_INVALID_TIMER)
				{
						arch_puts("Control Point 2 Activated\r\n");
						mcp9808_initiate_timer = app_easy_timer(I2C_DATA_CAPTURE_INITIATION_PERIOD_MCP9808, initiate_mcp9808_for_data_capture);
				}
				else
				{
						arch_puts("MCP9808 capture already running\r\n");
				}
    }
    else if(val == DISABLE_SENSOR_DATA_CAPTURING)
    {
        if (mcp9808_initiate_timer != EASY_TIMER_INVALID_TIMER)
        {
						arch_puts("Control Point 2 Deactivated -> Stopping MCP9808 capture initiation\r\n");
            app_easy_timer_cancel(mcp9808_initiate_timer);
            mcp9808_initiate_timer = EASY_TIMER_INVALID_TIMER;
        }
				else if (mcp9808_capture_timer != EASY_TIMER_INVALID_TIMER)
        {
						arch_puts("Control Point 2 Deactivated -> Stopping MCP9808 capture and notif\r\n");
            app_easy_timer_cancel(mcp9808_capture_timer);
            mcp9808_capture_timer = EASY_TIMER_INVALID_TIMER;
        }
				
				power_down_mcp9808();
    }
		else
				arch_puts("Invalid Value...Try 0x00 or 0x01\r\n");
}

void initiate_adxl345_for_data_capture()
{
		adxl345_initiate_timer = EASY_TIMER_INVALID_TIMER;
		
		i2c_init(&i2c_cfg_ADXL345);
		ADXL345_init();
		
	  // Wait until no master activity and if the bus is stuck reset it
		if (!i2c_wait_idle())
		{
				arch_puts("ADXL345 initialization: bus stuck, recovering\r\n");
				i2c_release();               /* release the controller BEFORE bit-banging */
				i2c_bus_recovery();
				diagnostic_data.i2c_rec_cnt++;
		}
		else{
			i2c_release();
		}
		
		adxl345_capture_timer = app_easy_timer(I2C_DATA_CAPTURE_NOTIF_PERIOD_ADXL345, adxl345_data_capture_and_notif);
}	

void initiate_mcp9808_for_data_capture(void)
{
		mcp9808_initiate_timer = EASY_TIMER_INVALID_TIMER;
		
		i2c_init(&i2c_cfg_MCP9808);
		MCP9808_init();
	
		// Wait until no master activity and if the bus is stuck reset it
		if (!i2c_wait_idle())
		{
				arch_puts("MCP9808 initialization: bus stuck, recovering\r\n");
				i2c_release();               /* release the controller BEFORE bit-banging */
				i2c_bus_recovery();
				diagnostic_data.i2c_rec_cnt++;
		}
		else{
			i2c_release();
		}
		
		mcp9808_capture_timer = app_easy_timer(I2C_DATA_CAPTURE_NOTIF_PERIOD_MCP9808, mcp9808_data_capture_and_notif);
}

void adxl345_data_capture_and_notif()
{
		adxl345_capture_timer = EASY_TIMER_INVALID_TIMER;

		i2c_init(&i2c_cfg_ADXL345);
	
		ADXL345_read_XYZ(xyz);
		x = (xyz[0] & 0xFF) | (((uint16_t)xyz[1]) << 8);
		y = (xyz[2] & 0xFF) | (((uint16_t)xyz[3]) << 8);
		z = (xyz[4] & 0xFF) | (((uint16_t)xyz[5]) << 8);
	
		ADXL345_deinit();
	
	  // Wait until no master activity and if the bus is stuck reset it
		if (!i2c_wait_idle())
		{
				arch_puts("ADXL345 capture/deinitialization: bus stuck, recovering\r\n");
				i2c_release();               /* release the controller BEFORE bit-banging */
				i2c_bus_recovery();
				diagnostic_data.i2c_rec_cnt++;
		}
		else{
			i2c_release();
		}
	
		arch_printf("X: %d, Y: %d, Z: %d\r\n", x, y, z);

		// Check if the Central is still Connected
		if(ke_state_get(TASK_APP) != APP_CONNECTED)
		{
				arch_puts("App Disconnected Unexpectedly ... disabling ADXL345 timer callback\r\n");
				adxl345_initiate_timer = EASY_TIMER_INVALID_TIMER;
			
				return;
		}
	
		// If the Central is still connected and has the Notifications enabled, then Notify it for the new Accelerometer Value
    if (acc_cccd == PRF_CLI_START_NTF)
    {
				// Update and send value of Accelerometer
				struct custs1_val_ntf_ind_req *req_acc = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
																																prf_get_task_from_id(TASK_ID_CUSTS1),
																																TASK_APP,
																																custs1_val_ntf_ind_req,
																																DEF_SVC1_ACC_DATA_CHAR_LEN);


				req_acc->conidx = app_connection_idx;
				req_acc->handle = SVC1_IDX_ACCELEROMETER_VAL;
				req_acc->length = DEF_SVC1_ACC_DATA_CHAR_LEN;
				req_acc->notification = true;
				
				memcpy(req_acc->value, xyz, DEF_SVC1_ACC_DATA_CHAR_LEN);

				KE_MSG_SEND(req_acc);
    }
		
		// Set the timer once again until Stop command is received in Control Characteristic
		adxl345_initiate_timer = app_easy_timer(I2C_DATA_CAPTURE_INITIATION_PERIOD_ADXL345, initiate_adxl345_for_data_capture);
}

void mcp9808_data_capture_and_notif()
{
		mcp9808_capture_timer = EASY_TIMER_INVALID_TIMER;

		i2c_init(&i2c_cfg_MCP9808);
	
    temp_tenths = MCP9808_get_temperature_tenths();

		MCP9808_deinit();
	
		// Wait until no master activity and if the bus is stuck reset it
		if (!i2c_wait_idle())
		{
				arch_puts("MCP9808 capture/deinitialization: bus stuck, recovering\r\n");
				i2c_release();               /* release the controller BEFORE bit-banging */
				i2c_bus_recovery();
				diagnostic_data.i2c_rec_cnt++;
		}
		else{
			i2c_release();
		}
		
		const char *sign = (temp_tenths < 0) ? "-" : "";
		uint16_t    mag  = (temp_tenths < 0) ? -temp_tenths : temp_tenths;

		arch_printf("Temp: %s%u.%u\r\n", sign, mag / 10, mag % 10);

		// Check if the Central is still Connected
		if(ke_state_get(TASK_APP) != APP_CONNECTED)
		{
				arch_puts("App Disconnected Unexpectedly ... disabling MCP9808 timer callback\r\n");
				mcp9808_initiate_timer = EASY_TIMER_INVALID_TIMER;
			
				return;
		}
		
		// If the Central is still connected and has the Notifications enabled, then Notify it for the new Temperature Value
    if (tmp_cccd == PRF_CLI_START_NTF)
    {
				struct custs1_val_ntf_ind_req *req_tmp = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
                                                          prf_get_task_from_id(TASK_ID_CUSTS1),
                                                          TASK_APP,
                                                          custs1_val_ntf_ind_req,
																													DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN);

				req_tmp->conidx = app_connection_idx;
				req_tmp->handle = SVC2_IDX_TEMPERATURE_VAL;
				req_tmp->length = DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN;
				req_tmp->notification = true;
				memcpy(req_tmp->value, &temp_tenths, sizeof(temp_tenths));

				KE_MSG_SEND(req_tmp);
    }
		
		// Set the timer once again until Stop command is received in Control Characteristic
		mcp9808_initiate_timer = app_easy_timer(I2C_DATA_CAPTURE_INITIATION_PERIOD_MCP9808, initiate_mcp9808_for_data_capture);
}

void request_connected_rssi(void)
{
		rssi_timer = EASY_TIMER_INVALID_TIMER;
	
    if(ke_state_get(TASK_APP) == APP_CONNECTED)
		{
        struct gapc_get_info_cmd *cmd = KE_MSG_ALLOC(GAPC_GET_INFO_CMD, 
																						 KE_BUILD_ID(TASK_GAPC, app_connection_idx),
																						 TASK_APP,
																						 gapc_get_info_cmd);

        cmd->operation = GAPC_GET_CON_RSSI;
        KE_MSG_SEND(cmd);
				
				rssi_timer = app_easy_timer(RSSI_READ_PERIOD,request_connected_rssi);
		}
}

void diagnostic_char_ind_handler(ke_msg_id_t const msgid,
																 struct custs1_value_req_ind const *param,
																 ke_task_id_t const dest_id,
																 ke_task_id_t const src_id)
{

		struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC_DYN(CUSTS1_VALUE_REQ_RSP,
                                                        prf_get_task_from_id(TASK_ID_CUSTS1),
                                                        TASK_APP,
                                                        custs1_value_req_rsp,
                                                        DEF_SVC3_DIAGNOSTIC_DATA_CHAR_LEN);
		
		
    rsp->conidx  = app_connection_idx;
    rsp->att_idx = param->att_idx;
    rsp->length  = DEF_SVC3_DIAGNOSTIC_DATA_CHAR_LEN;
    rsp->status  = ATT_ERR_NO_ERROR;
    
    memcpy(&rsp->value, &diagnostic_data, rsp->length);
	
		// Send message
    KE_MSG_SEND(rsp);
		
		arch_puts("Diagnostic RI characteristic read\r\n");
}

