/*
 * rtos_joystick.c
 *
 * Created: 6/9/2013 5:55:26 PM
 *  Author: Jonah Tsai
 */ 
// Test

#include "rtos_joystick.h"

#include <task.h>

#include "rtos_button.h"
#include "rtos_adc.h"
#include "tm_stick.h"
/*
#include "mlx90363.h"

*/

#ifdef __cplusplus
extern "C" {
#endif


rtos_joystick_t g_rtos_joystick_control = {
	.mutex = NULL,
};

void joystick_adc_task(void* parameters);
void joystick_button_task(void* parameters);
// void joystick_stick_task(void *parameters);

void rtos_joystick_init(bool is_init_tm_stick) {
	if(g_rtos_joystick_control.mutex == NULL) {
		g_rtos_joystick_control.mutex = xSemaphoreCreateMutex();
		// Initialize the USB report byte array. For now, make every thing 0
		// TODO: Should we allow initial values? These will certainly be for each button and each ADC values.
		for(size_t i = 0; i < UDI_HID_JOYSTICK_REPORT_SIZE; i++) {
			g_rtos_joystick_control.joystick_report[i] = 0;
		}
	}
	
	if(is_init_tm_stick) {
		tm_stick_init(500000);
	}
	
	rtos_button_init(CONF_TOTAL_NUM_BUTTONS, is_init_tm_stick);
	
	// mlx90363_init(2); // 500Hz so we not only conforms to MLX90363's timing, we get as much data rate as we can push it.
	// configure_time_trigger_for_adc(2000);
	xTaskHandle joystick_adc_task_handle;
	init_adc_data(ADC_PDC_ENABLE_MASK);
	
	if(g_adc_data.num_channel_enabled > 0) {
		start_adc(1000);
		xTaskCreate(joystick_adc_task, (const signed char*)"Joystick ADC Processing Task", configMINIMAL_STACK_SIZE * 2, NULL, ( ( unsigned portBASE_TYPE ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT, &joystick_adc_task_handle);
	}

	
	xTaskHandle joystick_button_task_handle;
	xTaskCreate(joystick_button_task, (const signed char*)"Joystick Processing Task", configMINIMAL_STACK_SIZE * 2, NULL, ( ( unsigned portBASE_TYPE ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT, &joystick_button_task_handle);
	
}


void joystick_adc_task(void* parameters) {
	for(;;) {
		xSemaphoreTake(g_adc_data.rtos_task_semaphore, portMAX_DELAY); // wait for notification
		xSemaphoreTake(g_rtos_joystick_control.mutex, portMAX_DELAY); // lock down the joystick report buffer
		
		xSemaphoreTake(g_adc_data.mutex, portMAX_DELAY); // lock down the data protection mutex	


		uint16_t adc0_value, adc1_value, adc2_value, adc3_value, adc4_value, adc5_value, adc6_value, adc7_value;
		
		adc0_value = g_adc_data.data[0];
		adc1_value = g_adc_data.data[1];
		adc2_value = g_adc_data.data[2];
		adc3_value = g_adc_data.data[3];
		adc4_value = g_adc_data.data[4];
		adc5_value = g_adc_data.data[5];
		adc6_value = g_adc_data.data[6];
		adc7_value = g_adc_data.data[7];
		
		// X
		g_rtos_joystick_control.joystick_report[8] = adc0_value & 0x00FF;
		g_rtos_joystick_control.joystick_report[9] = (adc0_value & 0xFF00) >> 8;
	
		// Y
		g_rtos_joystick_control.joystick_report[10] = adc1_value & 0x00FF;
		g_rtos_joystick_control.joystick_report[11] = (adc1_value & 0xFF00) >> 8;
	
		// Z
		g_rtos_joystick_control.joystick_report[12] = adc2_value & 0x00FF;
		g_rtos_joystick_control.joystick_report[13] = (adc2_value & 0xFF00) >> 8;
		
		// Rx
		g_rtos_joystick_control.joystick_report[14] = adc3_value & 0x00FF;
		g_rtos_joystick_control.joystick_report[15] = (adc3_value & 0xFF00) >> 8;
		
		// Ry
		g_rtos_joystick_control.joystick_report[16] = adc4_value & 0x00FF;
		g_rtos_joystick_control.joystick_report[17] = (adc4_value & 0xFF00) >> 8;
		
		// Rz
		g_rtos_joystick_control.joystick_report[18] = adc5_value & 0x00FF;
		g_rtos_joystick_control.joystick_report[19] = (adc5_value & 0xFF00) >> 8;
		
		// Slider
		g_rtos_joystick_control.joystick_report[20] = adc6_value & 0x00FF;
		g_rtos_joystick_control.joystick_report[21] = (adc6_value & 0xFF00) >> 8;
		
		// Dial
		g_rtos_joystick_control.joystick_report[22] = adc7_value & 0x00FF;
		g_rtos_joystick_control.joystick_report[23] = (adc7_value & 0xFF00) >> 8;

		xSemaphoreGive(g_adc_data.mutex);
		
		xSemaphoreGive(g_rtos_joystick_control.mutex);
		

	}
	
}


void joystick_button_task(void* parameters) {
	for(;;) {
		xSemaphoreTake(g_rtos_button_data.rtos_task_semaphore, portMAX_DELAY); // wait for notification
		
		xSemaphoreTake(g_rtos_joystick_control.mutex, portMAX_DELAY); // lock down the joystick report buffer		
		xSemaphoreTake(g_rtos_button_data.mutex, portMAX_DELAY);
		size_t array_max_idx = g_rtos_button_data.num_button / 8;
		if((g_rtos_button_data.num_button % 8) == 0) {
			array_max_idx--;
		}
		uint8_t current_button_value = 0;
		for(size_t i = 0; i <= array_max_idx; i++) {
			current_button_value = g_rtos_button_data.data[i];
			g_rtos_joystick_control.joystick_report[i] = current_button_value;
		}
		xSemaphoreGive(g_rtos_button_data.mutex);
		xSemaphoreGive(g_rtos_joystick_control.mutex);	
	}
}



// This will be called from a callback, from an interrupt, so don't add anything that's not interrupt handler kosher!
void joystick_send_report(void) {
	portBASE_TYPE xHigherTaskWoken = pdFALSE;
	xSemaphoreTakeFromISR(g_rtos_joystick_control.mutex, &xHigherTaskWoken);
	udi_hid_joystick_send_report(g_rtos_joystick_control.joystick_report);
	xSemaphoreGiveFromISR(g_rtos_joystick_control.mutex, &xHigherTaskWoken);
}


#ifdef __cplusplus
}
#endif