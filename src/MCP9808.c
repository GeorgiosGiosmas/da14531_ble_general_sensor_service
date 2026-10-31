/**
 ****************************************************************************************
 *
 * @file MCP9808.c
 *
 * @brief Peripheral project source code.
 *
 * Copyright (c) 2015-2021 Renesas Electronics Corporation and/or its affiliates
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 ****************************************************************************************
 */

/*
 * INCLUDE FILES                                                                        s
 ****************************************************************************************
 */
#include "MCP9808.h"
#include "i2c.h"

const uint16_t constant_temp = (uint16_t)((1U << TEMPERATURE_REGISTER_SIZE) - 1);
const uint16_t constant_reso = (uint16_t)((1U << RESOLUTION_CONFIG_REGISTER_SIZE) - 1);

/**
 ****************************************************************************************
 * @brief Initialize the temperature sensor, the function sets the resolution to maximum
 * @return void
 ****************************************************************************************
 */
void MCP9808_init(void){
	uint8_t buffer[3];
	i2c_abort_t abort_code = I2C_ABORT_NONE;
	
	buffer[0] = CONFIGURATION_REGISTER;
	buffer[1] = 0x00;
	buffer[2] = 0x00;
	
	i2c_master_transmit_buffer_sync((uint8_t*)buffer, sizeof(buffer), &abort_code, I2C_F_ADD_STOP);
	
	MCP9808_set_resolution_register(USED_TEMPERATURE_RESOLUTION);
}

/**
 ****************************************************************************************
 * @brief Deinitialize the temperature sensor, the function sets the resolution to maximum
 * @return void
 ****************************************************************************************
 */
void MCP9808_deinit(void){
	uint8_t buffer[3];
	i2c_abort_t abort_code = I2C_ABORT_NONE;
	
	buffer[0] = CONFIGURATION_REGISTER;
	buffer[1] = 0x01;
	buffer[2] = 0x00;
	
	i2c_master_transmit_buffer_sync((uint8_t*)buffer, sizeof(buffer), &abort_code, I2C_F_ADD_STOP);
}

/**
 ****************************************************************************************
 * @brief 		Sets the resolution register to resolution   
 * @param[in]	const temperature_resolution resolution
 * @return 		void
 ****************************************************************************************
 */
void MCP9808_set_resolution_register(const temperature_resolution resolution){
	uint8_t buffer[2];
	i2c_abort_t abort_code = I2C_ABORT_NONE;
	
	buffer[0] = RESOLUTION_CONFIG_REGISTER;
	buffer[1] = resolution;
	
	i2c_master_transmit_buffer_sync((uint8_t*)buffer, sizeof(buffer), &abort_code, I2C_F_ADD_STOP);
	
	if(abort_code != I2C_ABORT_NONE) {
		//insert error handler
	}
}

/**
 ****************************************************************************************
 * @brief Reads the resolution register
 * @return temperature_resolution
 ****************************************************************************************
 */
temperature_resolution MCP9808_get_resolution_register(void){
	uint8_t buffer[1];
	i2c_abort_t abort_code = I2C_ABORT_NONE;
	buffer[0] = RESOLUTION_CONFIG_REGISTER;
	
	i2c_master_transmit_buffer_sync((uint8_t*)buffer,sizeof(buffer), &abort_code, I2C_F_NONE);
	
	if(abort_code != I2C_ABORT_NONE) {
		//insert error handler
	}
	
	i2c_master_receive_buffer_sync((uint8_t*)buffer, sizeof(buffer), &abort_code, I2C_F_ADD_STOP);
	
	if(abort_code != I2C_ABORT_NONE) {
		//insert error handler
	}
	
	return (temperature_resolution)( buffer[0] & constant_reso ); 
}

/**
 ****************************************************************************************
 * @brief read the 16bits data from temperature register 5. 
 * @return uint16_t 
 ****************************************************************************************
 */
uint16_t MCP9808_read_temperature_register(void){

	uint8_t buffer[2];
	i2c_abort_t abort_code = I2C_ABORT_NONE;
		
	buffer[0] = TEMPERATURE_DATA_REGISTER; 
	
	i2c_master_transmit_buffer_sync((uint8_t*)buffer, 1, &abort_code, I2C_F_NONE);
	if(abort_code != I2C_ABORT_NONE) {
		//insert error handler
	}
	
	i2c_master_receive_buffer_sync((uint8_t*)buffer, sizeof(buffer), &abort_code, I2C_F_ADD_STOP);
	if(abort_code != I2C_ABORT_NONE) {
		//insert error handler
	}
	
	return ((buffer[0] << 8) + buffer[1]); 
}

/**
 ****************************************************************************************
 * @brief Combines reading the temperature register and calculating the temperature in celsius  
 * @return int16_t 
 ****************************************************************************************
 */
int16_t MCP9808_get_temperature_tenths(void){
	
	uint16_t reg  = MCP9808_read_temperature_register();
	uint16_t temp_data = reg & constant_temp;                 // SIGN + 12 data bits
	
	// Sign-extend bit 12 into bits 15..13
	int16_t t = (temp_data & 0x1000) ? (int16_t)(temp_data | 0xE000) : (int16_t)temp_data;

	return (int16_t)((t * 5) / 8);                // 1/16 °C -> tenths of °C
}

