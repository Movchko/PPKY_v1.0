/*
 * WEO012864A.c
 *
 *  Created on: Oct 28, 2025
 *      Author: 79099
 */

// WEO012864A - OLED Display 128x64 (SSD1309 compatible)
// SPI Interface

#include "Display/display.h"
#include "main.h"
#include "stm32h5xx_hal.h"

// External SPI handle from main.c
extern SPI_HandleTypeDef hspi3;

// Constants for display addressing
#define ADDR_MODE_HORIZONTAL  0x00
#define ADDR_MODE_VERTICAL    0x01
#define ADDR_MODE_PAGE        0x02
#define ADDR_MODE             0x02  // Page addressing mode

// Display dimensions
#define DISP_WIDTH  128
#define DISP_HEIGHT 64
#define DISP_PAGES  (DISP_HEIGHT / 8)


// Отправка команды на дисплей согласно даташиту SSD1309 (4-wire SPI)
// Порядок как в рабочем проекте h523_ppkiu_test
void Write_Command(uint8_t cmd) {
  HAL_GPIO_WritePin(DISP_CS_GPIO_Port, DISP_CS_Pin, GPIO_PIN_RESET);    // CS LOW
  __NOP(); __NOP();
  HAL_GPIO_WritePin(DISP_D_C_GPIO_Port, DISP_D_C_Pin, GPIO_PIN_RESET);    // D/C LOW (Command)
  
  HAL_SPI_Transmit(&hspi3, &cmd, 1, HAL_MAX_DELAY);  // Send command byte
  
  HAL_GPIO_WritePin(DISP_CS_GPIO_Port, DISP_CS_Pin, GPIO_PIN_SET);      // CS HIGH
  __NOP(); __NOP();
}

// Отправка данных на дисплей согласно даташиту SSD1309 (4-wire SPI)
// Порядок как в рабочем проекте h523_ppkiu_test
void Write_Data(uint8_t data) {
  HAL_GPIO_WritePin(DISP_CS_GPIO_Port, DISP_CS_Pin, GPIO_PIN_RESET);    // CS LOW
  __NOP(); __NOP();
  HAL_GPIO_WritePin(DISP_D_C_GPIO_Port, DISP_D_C_Pin, GPIO_PIN_SET);      // D/C HIGH (Data)
  
  HAL_SPI_Transmit(&hspi3, &data, 1, HAL_MAX_DELAY);  // Send data byte
  
  HAL_GPIO_WritePin(DISP_CS_GPIO_Port, DISP_CS_Pin, GPIO_PIN_SET);      // CS HIGH
  __NOP(); __NOP();
}


/**
 * @brief Set display page and column addresses for Page Addressing Mode
 * @param page Page address (0-7)
 * @param col_start Start column (0-127)
 * @param col_end End column (0-127) - not used in Page mode but kept for compatibility
 */
void Set_Addr(uint8_t page, uint8_t col_start, uint8_t col_end) {
	// For Page Addressing Mode, use 0xB0+i for page, then column address commands
	Write_Command(0xB0 | (page & 0x07)); // Set Page Start Address (page 0-7)
	
	// Column address split into lower (bits 0-3) and higher (bits 4-6)
	uint8_t col_low = col_start & 0x0F;  // Lower 4 bits
	uint8_t col_high = (col_start >> 4) & 0x07; // Upper 3 bits
	
	Write_Command(0x00 | col_low);  // Set Lower Column Start Address
	Write_Command(0x10 | col_high); // Set Higher Column Start Address
}

/**
 * @brief Hardware reset of display
 * RES#: Active LOW - when LOW resets the chip, keep HIGH during normal operation
 */
void Display_Reset(void) {
	HAL_GPIO_WritePin(DISP_RES_GPIO_Port, DISP_RES_Pin, GPIO_PIN_RESET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(DISP_RES_GPIO_Port, DISP_RES_Pin, GPIO_PIN_SET);
	HAL_Delay(100);
}

/**
 * @brief Initialize WEO012864A display
 * Используем последовательность из рабочего проекта h523_ppkiu_test
 */
void InitDisplay(void) {
	Display_Reset();
	HAL_Delay(500);  // Дать время дисплею на стабилизацию
	
	// Последовательность инициализации из рабочего проекта
	Write_Command(0xAE);       // Display OFF
	Write_Command(0xA4);       // ignores RAM
	Write_Command(0xA6);       // Normal display
	Write_Command(0xA8);       // Multiplex ratio
	Write_Command(0x3F);       // 63
	Write_Command(0xD3);       // Display offset
	Write_Command(0x00);
	Write_Command(0x40);       // Display start line
	Write_Command(0xA0);       // Segment remap //A1 revert
	Write_Command(0xC0);       // COM scan direction //C8 revert
	Write_Command(0xDA);       // COM pins
	Write_Command(0x12);
	Write_Command(0x81);       // Contrast
	Write_Command(0x7F);
	Write_Command(0xA4);       // Output follows RAM
	Write_Command(0xD5);       // Clock divide
	Write_Command(0x80);
	Write_Command(0x8D);       // Charge pump
	Write_Command(0x14);
	Write_Command(0xAF);       // Display ON
	HAL_Delay(100);  // Дать время дисплею на включение
}

/**
 * @brief Update display from TouchGFX framebuffer
 * @param framebuffer Pointer to framebuffer at rect position (monochrome, 1 bit per pixel)
 *                    This pointer is already advanced to the rect start by TouchGFX
 * @param x X coordinate of rectangle (absolute)
 * @param y Y coordinate of rectangle (absolute)
 * @param width Width of rectangle
 * @param height Height of rectangle
 */
void Display_UpdateRect(uint8_t *framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
	// Simple test: just fill white
	//Display_TestFill();
	//return;
	
	if (framebuffer == NULL) return;
	
	// Clamp coordinates
	if (x >= DISP_WIDTH) return;
	if (y >= DISP_HEIGHT) return;
	if ((x + width) > DISP_WIDTH) width = DISP_WIDTH - x;
	if ((y + height) > DISP_HEIGHT) height = DISP_HEIGHT - y;
	if (width == 0 || height == 0) return;
	
	// TouchGFX framebuffer stride (bytes per row)
	uint16_t fb_stride = (DISP_WIDTH + 7) / 8; // 16 bytes per row for 128px width
	
	// Calculate page addresses for display
	uint8_t page_start = y / 8;
	uint8_t page_end = (y + height - 1) / 8;
	uint8_t col_start = x;
	uint8_t col_end = x + width - 1;
	
	// Process each page
	for (uint8_t page = page_start; page <= page_end; page++) {
		// Set display address for this page
		Set_Addr(page, col_start, col_end);
		
		// Calculate vertical range within this page
		uint8_t page_row_start = (page == page_start) ? (y % 8) : 0;
		uint8_t page_row_end = (page == page_end) ? ((y + height - 1) % 8) : 7;
		
		// For each column in the rectangle
		for (uint16_t col = 0; col < width; col++) {
			uint8_t pixel_data = 0;
			
			// Convert from TouchGFX format (row-by-row) to SSD1309 format (page-by-page)
			// framebuffer points to the rect start, so we need to calculate relative addresses
			for (uint8_t bit = 0; bit < 8; bit++) {
				// Absolute display row
				uint16_t abs_row = page * 8 + bit;
				
				// Check if this row is within the rectangle bounds
				if (abs_row < y || abs_row >= (y + height)) {
					continue;
				}
				
				// Check if this bit is within the current page range
				if (bit < page_row_start || bit > page_row_end) {
					continue;
				}
				
				// Relative row within the rectangle (0 to height-1)
				uint16_t rel_row = abs_row - y;
				
				// Absolute column
				uint16_t abs_col = x + col;
				
				// Calculate framebuffer byte and bit position
				// TouchGFX: MSB first, 8 horizontal pixels per byte
				uint16_t fb_byte_idx = rel_row * fb_stride + (abs_col / 8);
				uint8_t fb_bit_pos = 7 - (abs_col % 8);
				
				// Extract pixel from framebuffer
				if (fb_byte_idx < (height * fb_stride)) {
					uint8_t fb_byte = framebuffer[fb_byte_idx];
					if (fb_byte & (1 << fb_bit_pos)) {
						pixel_data |= (1 << bit); // SSD1309: LSB = row 0
					}
				}
			}
			
			Write_Data(pixel_data);
		}
	}
}

/**
 * @brief Update full display from TouchGFX framebuffer
 * @param framebuffer Pointer to framebuffer (monochrome, 1 bit per pixel)
 */

