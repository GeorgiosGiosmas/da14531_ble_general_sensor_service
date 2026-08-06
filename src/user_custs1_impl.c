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

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

ke_msg_id_t timer_adxl345_used      __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
ke_msg_id_t timer_mcp9808_used      __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t indication_counter 				__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t non_db_val_counter 				__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
int previous_temp_int 							__SECTION_ZERO("retention_mem_area0");
int previous_temp_frac							__SECTION_ZERO("retention_mem_area0");
char temperature_string[DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN]  __SECTION_ZERO("retention_mem_area0");
int16_t previous_accel_x 							__SECTION_ZERO("retention_mem_area0");
int16_t previous_accel_y							__SECTION_ZERO("retention_mem_area0");
int16_t previous_accel_z 							__SECTION_ZERO("retention_mem_area0");
uint8_t previous_accel_xyz[DEF_SVC1_GYR_DATA_CHAR_LEN] 			__SECTION_ZERO("retention_mem_area0");

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
        timer_adxl345_used = app_easy_timer(I2C_DATA_CAPTURE_PERIOD, capture_adxl345_data_cb_handler);
    }
    else if(val == DISABLE_SENSOR_DATA_CAPTURING)
    {
        if (timer_adxl345_used != EASY_TIMER_INVALID_TIMER)
        {
            app_easy_timer_cancel(timer_adxl345_used);
            timer_adxl345_used = EASY_TIMER_INVALID_TIMER;
        }
    }
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
        timer_mcp9808_used = app_easy_timer(I2C_DATA_CAPTURE_PERIOD, capture_mcp9808_data_cb_handler);
    }
    else if(val == DISABLE_SENSOR_DATA_CAPTURING)
    {
        if (timer_mcp9808_used != EASY_TIMER_INVALID_TIMER)
        {
            app_easy_timer_cancel(timer_mcp9808_used);
            timer_mcp9808_used = EASY_TIMER_INVALID_TIMER;
        }
    }
}

/**
 ****************************************************************************************
 * @brief Helper function to convert a raw measurement to a string.
 * @param[in]  input   Input raw measurement value
 * @param[out] s       Pointer to the output string
 * @return Length of string
 ****************************************************************************************
 */
static uint8_t user_int_to_string(int16_t input, uint8_t *s){
	uint8_t length = 1;
	if(input < 0){
		s[0] = '-';
	} else {
		s[0] = ' ';
	}
	input = abs(input);
	if(input  >= 10000){
		s[length++] = '0' + ((input / 10000) % 10);
	}
	if(input  >= 1000){
		s[length++] = '0' + ((input / 1000) % 10);
	}
	if(input  >= 100){
		s[length++] = '0' + ((input / 100) % 10);
	}
	if(input  >= 10){
		s[length++] = '0' + ((input / 10) % 10);
	}
	
	s[length++] = '0' + (input% 10);
	return length;
}

// Function that initiates ADXL345 and captures data
static void adxl345_capture(int16_t *x, int16_t *y, int16_t *z, uint8_t *xyz)
{
		i2c_init(&i2c_cfg_ADXL345);
	
		ADXL345_init();
	
		*x = ADXL345_read_X();
		*y = ADXL345_read_Y();
		*z = ADXL345_read_Z();
		ADXL345_read_XYZ(xyz);
		
		i2c_release();
}	

// Function that initiates MCP9808 and captures data
static void mcp9808_capture(int *temp_int, int *temp_frac)
{
		i2c_init(&i2c_cfg_MCP9808);
	
		MCP9808_init();
	
		double temperature = MCP9808_get_temperature();
		*temp_int = (int)temperature;
		*temp_frac = (int)((temperature - *temp_int) * 10000);
	
		i2c_release();
}

void capture_adxl345_data_cb_handler()
{
    // ADXL345 Data capturing
    int16_t x, y, z;
		uint8_t axis_val[DEF_SVC1_ACCEL_X_DATA_CHAR_LEN];
		uint8_t xyz[DEF_SVC1_GYR_DATA_CHAR_LEN];
	
		adxl345_capture(&x, &y, &z, xyz);
		previous_accel_x = x;
		previous_accel_y = y;
		previous_accel_z = z;
		memcpy(previous_accel_xyz, xyz, sizeof(DEF_SVC1_GYR_DATA_CHAR_LEN));

		// Update and send value of Accel X
		struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
                                                          prf_get_task_from_id(TASK_ID_CUSTS1),
                                                          TASK_APP,
                                                          custs1_val_ntf_ind_req,
                                                          DEF_SVC1_ACCEL_X_DATA_CHAR_LEN);


    req->conidx = 0;
    req->handle = SVC1_IDX_ACCELEROMETER_X_VAL;
    req->length = DEF_SVC1_ACCEL_X_DATA_CHAR_LEN;
    req->notification = true;
		
		uint8_t string_length = user_int_to_string(x * 3.9, axis_val);    //Read data and multiply by 3.9 to get acceleration in mg
    memcpy(req->value, axis_val, DEF_SVC1_ACCEL_X_DATA_CHAR_LEN);

    KE_MSG_SEND(req);
		
		memset(req, 0, sizeof(*req));
		memset(axis_val, 0, sizeof(axis_val));
		
		// Update and send value of Accel Y
		req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
														prf_get_task_from_id(TASK_ID_CUSTS1),
														TASK_APP,
														custs1_val_ntf_ind_req,
														DEF_SVC1_ACCEL_Y_DATA_CHAR_LEN);


    req->conidx = 0;
    req->handle = SVC1_IDX_ACCELEROMETER_Y_VAL;
    req->length = DEF_SVC1_ACCEL_Y_DATA_CHAR_LEN;
    req->notification = true;
		
		string_length = user_int_to_string(y * 3.9, axis_val);    //Read data and multiply by 3.9 to get acceleration in mg
    memcpy(req->value, axis_val, DEF_SVC1_ACCEL_Y_DATA_CHAR_LEN);

    KE_MSG_SEND(req);
		
		memset(req, 0, sizeof(*req));
		memset(axis_val, 0, sizeof(axis_val));
		
		// Update and send value of Accel Z
		req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
														prf_get_task_from_id(TASK_ID_CUSTS1),
														TASK_APP,
														custs1_val_ntf_ind_req,
														DEF_SVC1_ACCEL_Z_DATA_CHAR_LEN);


    req->conidx = 0;
    req->handle = SVC1_IDX_ACCELEROMETER_Z_VAL;
    req->length = DEF_SVC1_ACCEL_Z_DATA_CHAR_LEN;
    req->notification = true;
		
		string_length = user_int_to_string(z * 3.9, axis_val);    //Read data and multiply by 3.9 to get acceleration in mg
    memcpy(req->value, axis_val, DEF_SVC1_ACCEL_Z_DATA_CHAR_LEN);

    KE_MSG_SEND(req);
		
		memset(req, 0, sizeof(*req));
		memset(axis_val, 0, sizeof(axis_val));
		
		// Update and send value of Accel G
		req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
														prf_get_task_from_id(TASK_ID_CUSTS1),
														TASK_APP,
														custs1_val_ntf_ind_req,
														DEF_SVC1_GYR_DATA_CHAR_LEN);


    req->conidx = 0;
    req->handle = SVC1_IDX_GYROSCOPE_VAL;
    req->length = DEF_SVC1_GYR_DATA_CHAR_LEN;
    req->notification = true;
		
    memcpy(req->value, xyz, DEF_SVC1_GYR_DATA_CHAR_LEN);

    KE_MSG_SEND(req);

    if (ke_state_get(TASK_APP) == APP_CONNECTED)
    {
        // Set it once again until Stop command is received in Control Characteristic
        timer_adxl345_used = app_easy_timer(I2C_DATA_CAPTURE_PERIOD, capture_adxl345_data_cb_handler);
    }
}

void capture_mcp9808_data_cb_handler()
{
    struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
                                                          prf_get_task_from_id(TASK_ID_CUSTS1),
                                                          TASK_APP,
                                                          custs1_val_ntf_ind_req,
                                                          DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN);

    // MCP9808 Data Capturing
    int temp_int, temp_frac;
		mcp9808_capture(&temp_int, &temp_frac);
		previous_temp_int = temp_int;
		previous_temp_frac = temp_frac;
		
		uint8_t length = snprintf(temperature_string,DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN, "%d.%04d" ,temp_int, temp_frac);

		req->conidx = 0;
    req->handle = SVC2_IDX_TEMPERATURE_VAL;
    req->length = DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN;
    req->notification = true;
    memcpy(req->value, temperature_string, DEF_SVC2_TEMPERATURE_VAL_CHAR_LEN);

    KE_MSG_SEND(req);

    if (ke_state_get(TASK_APP) == APP_CONNECTED)
    {
        // Set it once again until Stop command is received in Control Characteristic
        timer_mcp9808_used = app_easy_timer(I2C_DATA_CAPTURE_PERIOD, capture_mcp9808_data_cb_handler);
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
