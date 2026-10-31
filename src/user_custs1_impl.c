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

ke_msg_id_t timer_adxl345_used      												__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
ke_msg_id_t timer_mcp9808_used      												__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t indication_counter 																__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t non_db_val_counter 																__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

extern const i2c_cfg_t i2c_cfg_MCP9808;
extern const i2c_cfg_t i2c_cfg_ADXL345;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

void user_svc1_ctrl_wr_ind_handler(ke_msg_id_t const msgid,
                                      struct custs1_val_write_ind const *param,
                                      ke_task_id_t const dest_id,
                                      ke_task_id_t const src_id)
{
    uint8_t val = 0;
    memcpy(&val, &param->value[0], param->length);

    if (val == ENABLE_SENSOR_DATA_CAPTURING)
    {
				arch_puts("Control Point 1 Activated\r\n");
        timer_adxl345_used = app_easy_timer(I2C_DATA_CAPTURE_PERIOD_ADXL345, capture_adxl345_data_cb_handler);
    }
    else if(val == DISABLE_SENSOR_DATA_CAPTURING)
    {
        if (timer_adxl345_used != EASY_TIMER_INVALID_TIMER)
        {
						arch_puts("Control Point 1 Deactivated\r\n");
            app_easy_timer_cancel(timer_adxl345_used);
            timer_adxl345_used = EASY_TIMER_INVALID_TIMER;
        }
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
				arch_puts("Control Point 2 Activated\r\n");
        timer_mcp9808_used = app_easy_timer(I2C_DATA_CAPTURE_PERIOD_MCP9808, capture_mcp9808_data_cb_handler);
    }
    else if(val == DISABLE_SENSOR_DATA_CAPTURING)
    {
        if (timer_mcp9808_used != EASY_TIMER_INVALID_TIMER)
        {
						arch_puts("Control Point 2 Deactivated\r\n");
            app_easy_timer_cancel(timer_mcp9808_used);
            timer_mcp9808_used = EASY_TIMER_INVALID_TIMER;
        }
    }
		else
				arch_puts("Invalid Value...Try 0x00 or 0x01\r\n");
}

/** Function that initializes and captures ADXL345 data **/
static void adxl345_capture(int16_t *x, int16_t *y, int16_t *z, uint8_t *xyz)
{
		i2c_init(&i2c_cfg_ADXL345);
		ADXL345_init();
	
		arch_asm_delay_us(11000);
	
		ADXL345_read_XYZ(xyz);
		*x = (*xyz & 0xFF) | (((uint16_t)*(xyz+1)) << 8);
		*y = (*(xyz+2) & 0xFF) | (((uint16_t)*(xyz+3)) << 8);
		*z = (*(xyz+4) & 0xFF) | (((uint16_t)*(xyz+5)) << 8);
	
		ADXL345_deinit();
	
	  // Wait until no master activity
		while (i2c_is_master_busy());
	
		i2c_release();
}	

/** Function that initializes and captures MCP9800 data **/
static void mcp9808_capture(int16_t *temp_tenths)
{
		i2c_init(&i2c_cfg_MCP9808);
		MCP9808_init();
	
		arch_asm_delay_us(35000);
	
		*temp_tenths = MCP9808_get_temperature_tenths();

		MCP9808_deinit();
	
		// Wait until no master activity
		while (i2c_is_master_busy());
	
		i2c_release();
}

void capture_adxl345_data_cb_handler()
{
		// ADXL345 Data capturing
    int16_t x, y, z;
		uint8_t xyz[DEF_SVC1_ACC_DATA_CHAR_LEN];
	
		adxl345_capture(&x, &y, &z, xyz);
	
		arch_printf("X: %d, Y: %d, Z: %d\r\n", x, y, z);
		
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

    if (ke_state_get(TASK_APP) == APP_CONNECTED)
    {
        // Set it once again until Stop command is received in Control Characteristic
        timer_adxl345_used = app_easy_timer(I2C_DATA_CAPTURE_PERIOD_ADXL345, capture_adxl345_data_cb_handler);
    }
		else
		{
				arch_puts("App Disconnected Upexpectedly ... disabling ADXL345 timer callback\r\n");
				app_easy_timer_cancel(timer_adxl345_used);
				timer_adxl345_used = EASY_TIMER_INVALID_TIMER;
		}
}

void capture_mcp9808_data_cb_handler()
{
    struct custs1_val_ntf_ind_req *req_tmp = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
                                                          prf_get_task_from_id(TASK_ID_CUSTS1),
                                                          TASK_APP,
                                                          custs1_val_ntf_ind_req,
                                                          DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN);

		// MCP9808 Data Capturing
    int16_t temp_tenths;
		mcp9808_capture(&temp_tenths);
		
		const char *sign = (temp_tenths < 0) ? "-" : "";
		uint16_t    mag  = (temp_tenths < 0) ? -temp_tenths : temp_tenths;

		arch_printf("Temp: %s%u.%u\r\n", sign, mag / 10, mag % 10);

		req_tmp->conidx = app_connection_idx;
    req_tmp->handle = SVC2_IDX_TEMPERATURE_VAL;
    req_tmp->length = DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN;
    req_tmp->notification = true;
		memcpy(req_tmp->value, &temp_tenths, sizeof(temp_tenths));

    KE_MSG_SEND(req_tmp);

    if (ke_state_get(TASK_APP) == APP_CONNECTED)
    {
        // Set it once again until Stop command is received in Control Characteristic
        timer_mcp9808_used = app_easy_timer(I2C_DATA_CAPTURE_PERIOD_MCP9808, capture_mcp9808_data_cb_handler);
    }
		else
		{
				arch_puts("App Disconnected Upexpectedly ... disabling MCP9808 timer callback\r\n");
				app_easy_timer_cancel(timer_mcp9808_used);
				timer_mcp9808_used = EASY_TIMER_INVALID_TIMER;
		}
}

void user_svc3_read_non_db_val_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id)
{
    // Increase value by one
    non_db_val_counter++;

    struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC_DYN(CUSTS1_VALUE_REQ_RSP,
                                                        prf_get_task_from_id(TASK_ID_CUSTS1),
                                                        TASK_APP,
                                                        custs1_value_req_rsp,
                                                        0 /*DEF_SVC3_READ_VAL_4_CHAR_LEN*/);

    // Provide the connection index.
    rsp->conidx  = app_env[param->conidx].conidx;
    // Provide the attribute index.
    rsp->att_idx = param->att_idx;
    // Force current length to zero.
    rsp->length  = sizeof(non_db_val_counter);
    // Provide the ATT error code.
    rsp->status  = ATT_ERR_NO_ERROR;
    // Copy value
    memcpy(&rsp->value, &non_db_val_counter, rsp->length);
    // Send message
    KE_MSG_SEND(rsp);
}
