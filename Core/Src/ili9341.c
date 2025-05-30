	/*
 * ili9341.c
 *
 *  Created on: 22 ���. 2018 �.
 *      Author: Andriy Honcharenko
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "ili9341.h"

enum {
  MemoryAccessControlNormalOrder,
  MemoryAccessControlReverseOrder
} MemoryAccessControlRefreshOrder;

enum {
	MemoryAccessControlColorOrderRGB,
	MemoryAccessControlColorOrderBGR
} MemoryAccessControlColorOrder;

static lcdPropertiesTypeDef  lcdProperties = { ILI9341_PIXEL_WIDTH, ILI9341_PIXEL_HEIGHT, LCD_ORIENTATION_PORTRAIT,true, true };
static lcdFontPropTypeDef lcdFont = {COLOR_YELLOW, COLOR_BLACK, &Font16, 1};
static lcdCursorPosTypeDef cursorXY = {0, 0};

static unsigned char lcdPortraitConfig = 0;
static unsigned char lcdLandscapeConfig = 0;
static unsigned char lcdPortraitMirrorConfig = 0;
static unsigned char lcdLandscapeMirrorConfig = 0;

static void				lcdDrawPixels(uint16_t x, uint16_t y, uint16_t *data, uint32_t dataLength);
static void        		lcdReset(void);
static void        		lcdWriteCommand(unsigned char command);
static void             lcdWriteData(unsigned short data);
static unsigned short	lcdReadData(void);

static unsigned char    lcdBuildMemoryAccessControlConfig(
                                bool rowAddressOrder,
                                bool columnAddressOrder,
                                bool rowColumnExchange,
                                bool verticalRefreshOrder,
                                bool colorOrder,
                                bool horizontalRefreshOrder);

//------------------------------------
#define   ILI_DISPOFF         0x28       // Display OFF
#define   ILI_PCA             0xCB       // Power Control A (5 parametr體 8b)
#define   ILI_PCB             0xCF       // Power Control B (3 parametry 8b)
#define   ILI_DTCA_ic         0xE8       // Driver Timming Control A (3 parametry 8b) - for internal clock
#define   ILI_POSC            0xED       // Power On Sequence Control (4 parametry 8b)
#define   ILI_DTCB            0xEA       // Driver Timming Control B (2 parametry 8b)
#define   ILI_E3G             0xF2       // Enable 3G (1 parametr 8b)
#define   ILI_PRC             0xF7       // Pump Ratio Control (1 parametr 8b)
#define   ILI_PWCTRL1         0xC0       // Power Control 1 (1 parametr 8b)
#define   ILI_PWCTRL2         0xC1       // Power Control 2 (1 parametr 8b)
#define   ILI_VMCTRL1         0xC5       // VCOM Control 1 (2 parametry 8b)
#define   ILI_VMCTRL2         0xC7       // VCOM Control 2 (1 parametr 8b)
#define   ILI_MADCTL          0x36       // Memory Access Control (1 parametr 8b)
#define   ILI_PIXSET          0x3A       // COLMOD: Pixel Format Set (1 parametr 8b)
#define   ILI_FRMCTR1         0xB1       // Frame Rate Control (In Normal Mode/Full Colors) (2 parametry 8b)
#define   ILI_GAMSET          0x26       // Gamma Set (1 parametr 8b)
#define   ILI_PGAMCTRL        0xE0       // Positive Gamma Correction (15 parametr體 8b)
#define   ILI_NGAMCTRL        0xE1       // Negative Gamma Correction (15 parametr體 8b)
#define   ILI_DISPON          0x29       // Display ON
#define   ILI_CASET           0x2A       // Column Address Set (4 parametry 8b: SC[15..8], SC[7..0], EC[15..8], EC[7..0])
#define   ILI_PASET           0x2B       // Page (row) Address Set (4 parametry 8b: SC[15..8], SC[7..0], EC[15..8], EC[7..0])
#define   ILI_RAMWR           0x2C       // Memory Write (n parametr體 18b, wywo硑wana bez parametr體 po CASET i PASET)
#define   ILI_ETMOD           0xB7       // Entry Mode Set (1 parametr 8b)
#define   ILI_DISCTRL         0xB6       // Display Function Control (4 parametry 8b)
#define   ILI_SLPOUT          0x11       // Sleep Out
#define   LCD_DISPLAY_ON      0x29       /* Display on register */
#define   LCD_GRAM            0x2C       /* GRAM register */

/*---------------------------------------------------------------------*/
//------------------------------------
/*---------------------------------------------------------------------*/
static const uint8_t init_tab[] = {

		ILI9341_DISPLAYOFF, 0, \
        ILI_PCB, 3, 0x00, 0x83, 0X30,  \
        ILI_POSC, 4, 0x64, 0x03, 0X12, 0X81,  \
        ILI_DTCA_ic, 3, 0x85, 0x01, 0x79,  \
        ILI_PCA, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,  \
        ILI_PRC,1, 0x20,  \
        ILI_DTCB, 2, 0x00, 0x00,  \
        ILI_PWCTRL1, 1, 0x26,  \
        ILI_PWCTRL2,1, 0x11,  \
        ILI_VMCTRL1, 2, 0x35, 0x3e,  \
        ILI_VMCTRL2, 1, 0xBE,  \
        ILI_MADCTL, 1, 0x00,  \
        ILI_PIXSET, 1, 0x55,  \
        ILI_FRMCTR1, 2, 0x00, 0x1B,  \
        ILI_E3G, 1, 0x08,  \
		ILI_GAMSET, 1, 0x01,  \
        ILI_PGAMCTRL, 15, 0x1F, 0x1A, 0x18, 0x0a, 0x0f, 0x06, 0x56, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00,  \
        ILI_NGAMCTRL, 15, 0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3a, 0x78, 0x4d, 0x05, 0x18, 0x0d, 0x38, 0x3a, 0x1F,  \
        ILI_CASET, 4, 0x00, 0x00, 0x00, 0xef,  \
		ILI_PASET, 4, 0x00, 0x00, 0x01, 0x3f,  \
        ILI9341_ENTRYMODE, 1, 0x07, \
        ILI_DISCTRL, 4, 0x0A, 0x82, 0x27, 0x00,  \
        ILI_SLPOUT, 120,  \
        ILI_DISPON, 100,  \
		LCD_GRAM, 0, \
        0x00
};

/*-------------------------------------------------------------------------------------------------------
*  EXECUTION CODE
-------------------------------------------------------------------------------------------------------*/
/********************************************************************************************************
*  Function: Driver_LcdReset
*  Object: lcd reset control
*  Input: sta
*  Output: none
*  brief: none
********************************************************************************************************/
/*****************************************************************************************************
 *
 * initialize sequence ILI_9341
 *
 ***************************************************************************************************/
void LCD_ILI9341_init(void)
{
    uint32_t i = 0;
    uint32_t i2;
    uint8_t counter;



    lcdPortraitConfig = lcdBuildMemoryAccessControlConfig(
                                                      MemoryAccessControlNormalOrder,		// rowAddressOrder
                                                      MemoryAccessControlReverseOrder,	// columnAddressOrder
                                                      MemoryAccessControlNormalOrder,		// rowColumnExchange
                                                      MemoryAccessControlNormalOrder,		// verticalRefreshOrder
                                                      MemoryAccessControlColorOrderBGR,	// colorOrder
                                                      MemoryAccessControlNormalOrder);	// horizontalRefreshOrder

    lcdLandscapeConfig = lcdBuildMemoryAccessControlConfig(
                                                      MemoryAccessControlNormalOrder,		// rowAddressOrder
                                                      MemoryAccessControlNormalOrder,		// columnAddressOrder
                                                      MemoryAccessControlReverseOrder,	// rowColumnExchange
                                                      MemoryAccessControlNormalOrder,		// verticalRefreshOrder
                                                      MemoryAccessControlColorOrderBGR,	// colorOrder
                                                      MemoryAccessControlNormalOrder);	// horizontalRefreshOrder

    lcdPortraitMirrorConfig = lcdBuildMemoryAccessControlConfig(
  		  	  	  	  	  	  	  	  	  	  	  	MemoryAccessControlReverseOrder,	// rowAddressOrder
  		                                            MemoryAccessControlNormalOrder,		// columnAddressOrder
  		                                            MemoryAccessControlNormalOrder,		// rowColumnExchange
  		                                            MemoryAccessControlNormalOrder,		// verticalRefreshOrder
  		                                            MemoryAccessControlColorOrderBGR,	// colorOrder
  		                                            MemoryAccessControlNormalOrder);	// horizontalRefreshOrder

    lcdLandscapeMirrorConfig = lcdBuildMemoryAccessControlConfig(
                                                      MemoryAccessControlReverseOrder,	// rowAddressOrder
                                                      MemoryAccessControlReverseOrder,	// columnAddressOrder
                                                      MemoryAccessControlReverseOrder,	// rowColumnExchange
                                                      MemoryAccessControlNormalOrder,		// verticalRefreshOrder
                                                      MemoryAccessControlColorOrderBGR,	// colorOrder
                                                      MemoryAccessControlNormalOrder);	// horizontalRefreshOrder

    lcdReset();

    while(init_tab[i] != 0)
    {
    	lcdWriteCommand((uint16_t)init_tab[i] );
        counter = init_tab[i+1];
        i += 2;
        if(counter > 15)
        {
            HAL_Delay(counter);
        }
        else if(counter > 0)
        {
            for(i2 = 0; i2 < counter ; i2++)
            {
            	lcdWriteData(init_tab[i+i2]) ;
            }
            i += counter;
        }
    }


    /*****************/
    lcdFillRGB(0);  // init screen as black
    //--------------
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET) ;  // enable LCD back light
    //--------------
}

void lcdTest(void)
{
	lcdSetWindow(0, 0, lcdProperties.width - 1, lcdProperties.height - 1);

	uint8_t stripSize = lcdProperties.height / 8;

	for (int y = 0; y < lcdProperties.height; y++)
	{
		for (int x = 0; x < lcdProperties.width; x++)
		{
		  if (y > lcdProperties.height - 1 - (stripSize * 1)) lcdWriteData(COLOR_WHITE);
		  else if (y > lcdProperties.height - 1 - (stripSize * 2)) lcdWriteData(COLOR_BLUE);
		  else if (y > lcdProperties.height - 1 - (stripSize * 3)) lcdWriteData(COLOR_GREEN);
		  else if (y > lcdProperties.height - 1 - (stripSize * 4)) lcdWriteData(COLOR_CYAN);
		  else if (y > lcdProperties.height - 1 - (stripSize * 5)) lcdWriteData(COLOR_RED);
		  else if (y > lcdProperties.height - 1 - (stripSize * 6)) lcdWriteData(COLOR_MAGENTA);
		  else if (y > lcdProperties.height - 1 - (stripSize * 7)) lcdWriteData(COLOR_YELLOW);
		  else lcdWriteData(COLOR_BLACK);
		}
	}
}

void lcdFillRGB(uint16_t color)
{
  lcdSetWindow(0, 0, lcdProperties.width - 1, lcdProperties.height - 1);
  int dimensions = lcdProperties.width * lcdProperties.height;
  while(dimensions--)
  {
    lcdWriteData(color);
  }
}

/**
 * \brief Draws a point at the specified coordinates
 *
 * \param x        x-Coordinate
 * \param y        y-Coordinate
 * \param color    Color
 *
 * \return void
 */
void lcdDrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    // Clip
    if ((x < 0) || (y < 0) || (x >= lcdProperties.width) || (y >= lcdProperties.height))
        return;

    lcdSetWindow(x, y, x, y);
    lcdWriteData(color);
}

void lcdDrawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint16_t color)
{
  // Allows for slightly better performance than setting individual pixels

	if (x1 < x0)
	{
		// Switch x1 and x0
		swap(x0, x1);
	}

	// Check limits
	if (x1 >= lcdProperties.width)
	{
		x1 = lcdProperties.width - 1;
	}

	if (x0 >= lcdProperties.width)
	{
		x0 = lcdProperties.width - 1;
	}

	lcdSetWindow(x0, y, x1, y);

	for (int line = x0; line <= x1; line++)
	{
		lcdWriteData(color);
	}
}

void lcdDrawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color)
{
  if (y1 < y0)
  {
	  swap(y0, y1);
  }

  if (x >= lcdProperties.width)
  {
    x = lcdProperties.width - 1;
  }

  if (y0 >= lcdProperties.height)
  {
    y0 = lcdProperties.height - 1;
  }

  if (y1 >= lcdProperties.height)
  {
    y1 = lcdProperties.height - 1;
  }

  lcdSetWindow(x, y0, x, y1);

  for(int line = y0; line <= y1; line++)
  {
	  lcdWriteData(color);
  }
}

/**
 * \brief Draws a line connecting the two points specified by the coordinate pairs
 *
 * \param x0	The x-coordinate of the first point
 * \param y0	The y-coordinate of the first point
 * \param x1	The x-coordinate of the second point
 * \param y1	The y-coordinate of the second point.
 * \param color	Color
 *
 * \return void
 */
void lcdDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
	// Bresenham's algorithm - thx wikpedia

	int16_t steep = abs(y2 - y1) > abs(x2 - x1);
	if (steep)
	{
		swap(x1, y1);
		swap(x2, y2);
	}

	if (x1 > x2)
	{
		swap(x1, x2);
		swap(y1, y2);
	}

	int16_t dx, dy;
	dx = x2 - x1;
	dy = abs(y2 - y1);

	int16_t err = dx / 2;
	int16_t ystep;

	if (y1 < y2)
	{
		ystep = 1;
	}
	else
	{
		ystep = -1;
	}

	for (; x1 <= x2; x1++)
	{
		if (steep)
		{
			lcdDrawPixel(y1, x1, color);
		}
		else
		{
			lcdDrawPixel(x1, y1, color);
		}
		err -= dy;
		if (err < 0)
		{
			y1 += ystep;
			err += dx;
		}
	}
}

/**
 * \brief Draws a rectangle specified by a coordinate pair, a width, and a height.
 *
 * \param x			The x-coordinate of the upper-left corner of the rectangle to draw
 * \param y			The y-coordinate of the upper-left corner of the rectangle to draw
 * \param w			Width of the rectangle to draw
 * \param h			Height of the rectangle to draw
 * \param color		Color
 *
 * \return void
 */
void lcdDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
	lcdDrawHLine(x, x + w - 1, y, color);
	lcdDrawHLine(x, x + w - 1, y + h - 1, color);
	lcdDrawVLine(x, y, y + h - 1, color);
	lcdDrawVLine(x + w - 1, y, y + h - 1, color);
}

/**
 * \brief Draws a rectangle with rounded corners specified by a coordinate pair, a width, and a height.
 *
 * \param x			The x-coordinate of the upper-left corner of the rectangle to draw
 * \param y			The y-coordinate of the upper-left corner of the rectangle to draw
 * \param w			Width of the rectangle to draw
 * \param h			Height of the rectangle to draw
 * \param r			Radius
 * \param color		Color
 *
 * \return void
 */
void lcdDrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
	// smarter version
	lcdDrawHLine(x + r, x + w - r, y, color);
	lcdDrawHLine(x + r, x + w - r, y + h - 1, color);
	lcdDrawVLine(x, y + r, y + h - r, color);
	lcdDrawVLine(x + w - 1, y + r, y + h - r, color);

	// draw four corners
	lcdDrawCircleHelper(x + r, y + r, r, 1, color);
	lcdDrawCircleHelper(x + w - r - 1, y + r, r, 2, color);
	lcdDrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
	lcdDrawCircleHelper(x + r, y + h - r - 1, r, 8, color);
}

/**
 * \brief Helper function drawing rounded corners
 *
 * \param x0			The x-coordinate
 * \param y0			The y-coordinate
 * \param r				Radius
 * \param cornername	Corner (1, 2, 3, 4)
 * \param color			Color
 *
 * \return void
 */
void lcdDrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	while (x < y)
	{
		if (f >= 0)
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;
		if (cornername & 0x4)
		{
			lcdDrawPixel(x0 + x, y0 + y, color);
			lcdDrawPixel(x0 + y, y0 + x, color);
		}
		if (cornername & 0x2)
		{
			lcdDrawPixel(x0 + x, y0 - y, color);
			lcdDrawPixel(x0 + y, y0 - x, color);
		}
		if (cornername & 0x8)
		{
			lcdDrawPixel(x0 - y, y0 + x, color);
			lcdDrawPixel(x0 - x, y0 + y, color);
		}
		if (cornername & 0x1)
		{
			lcdDrawPixel(x0 - y, y0 - x, color);
			lcdDrawPixel(x0 - x, y0 - y, color);
		}
	}
}

/**
 * \brief Draws an circle defined by a pair of coordinates and radius
 *
 * \param x0		The x-coordinate
 * \param y0		The y-coordinate
 * \param r			Radius
 * \param color		Color
 *
 * \return void
 */
void lcdDrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	lcdDrawPixel(x0, y0 + r, color);
	lcdDrawPixel(x0, y0 - r, color);
	lcdDrawPixel(x0 + r, y0, color);
	lcdDrawPixel(x0 - r, y0, color);

	while (x < y)
	{
		if (f >= 0)
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		lcdDrawPixel(x0 + x, y0 + y, color);
		lcdDrawPixel(x0 - x, y0 + y, color);
		lcdDrawPixel(x0 + x, y0 - y, color);
		lcdDrawPixel(x0 - x, y0 - y, color);
		lcdDrawPixel(x0 + y, y0 + x, color);
		lcdDrawPixel(x0 - y, y0 + x, color);
		lcdDrawPixel(x0 + y, y0 - x, color);
		lcdDrawPixel(x0 - y, y0 - x, color);
	}
}

/**************************************************************************/
/*!
   @brief   Draw a triangle with no fill color
    @param    x0  Vertex #0 x coordinate
    @param    y0  Vertex #0 y coordinate
    @param    x1  Vertex #1 x coordinate
    @param    y1  Vertex #1 y coordinate
    @param    x2  Vertex #2 x coordinate
    @param    y2  Vertex #2 y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void lcdDrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    lcdDrawLine(x0, y0, x1, y1, color);
    lcdDrawLine(x1, y1, x2, y2, color);
    lcdDrawLine(x2, y2, x0, y0, color);
}

/**
 * \brief Draws a filled circle defined by a pair of coordinates and radius
 *
 * \param x0		The x-coordinate
 * \param y0		The y-coordinate
 * \param r			Radius
 * \param color		Color
 *
 * \return void
 */
void lcdFillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
	lcdDrawVLine(x0, y0 - r, y0 + r + 1, color);
	lcdFillCircleHelper(x0, y0, r, 3, 0, color);
}

/**
 * \brief Helper function to draw a filled circle
 *
 * \param x0			The x-coordinate
 * \param y0			The y-coordinate
 * \param r				Radius
 * \param cornername	Corner (1, 2, 3, 4)
 * \param delta			Delta
 * \param color			Color
 *
 * \return void
 */
void lcdFillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	while (x < y)
	{
		if (f >= 0)
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		if (cornername & 0x1)
		{
			lcdDrawVLine(x0 + x, y0 - y, y0 + y + 1 + delta, color);
			lcdDrawVLine(x0 + y, y0 - x, y0 + x + 1 + delta, color);
		}
		if (cornername & 0x2)
		{
			lcdDrawVLine(x0 - x, y0 - y, y0 + y + 1 + delta, color);
			lcdDrawVLine(x0 - y, y0 - x, y0 + x + 1 + delta, color);
		}
	}
}

/**
 * \brief Draws a filled rectangle specified by a coordinate pair, a width, and a height.
 *
 * \param x				The x-coordinate of the upper-left corner of the rectangle to draw
 * \param y				The y-coordinate of the upper-left corner of the rectangle to draw
 * \param w				Width of the rectangle to draw
 * \param h				Height of the rectangle to draw
 * \param fillcolor		Color
 *
 * \return void
 */
void lcdFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillcolor)
{
	// clipping
	if((x >= lcdProperties.width) || (y >= lcdProperties.height)) return;
	if((x + w - 1) >= lcdProperties.width) w = lcdProperties.width - x;
	if((y + h - 1) >= lcdProperties.height) h = lcdProperties.height - y;

	for(int16_t y1 = y; y1 <= y + h; y1++)
	{
		lcdDrawHLine(x, x + w, y1, fillcolor);
	}
}

/**
 * \brief Draws a filled rounded rectangle specified by a coordinate pair, a width, and a height.
 *
 * \param x				The x-coordinate of the upper-left corner of the rectangle to draw
 * \param y				The y-coordinate of the upper-left corner of the rectangle to draw
 * \param w				Width of the rectangle to draw
 * \param h				Height of the rectangle to draw
 * \param r				Radius
 * \param fillcolor		Color
 *
 * \return void
 */
void lcdFillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
	// smarter version
	lcdFillRect(x + r, y, w - 2 * r, h, color);

	// draw four corners
	lcdFillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
	lcdFillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}


/**************************************************************************/
/*!
   @brief     Draw a triangle with color-fill
    @param    x0  Vertex #0 x coordinate
    @param    y0  Vertex #0 y coordinate
    @param    x1  Vertex #1 x coordinate
    @param    y1  Vertex #1 y coordinate
    @param    x2  Vertex #2 x coordinate
    @param    y2  Vertex #2 y coordinate
    @param    color 16-bit 5-6-5 Color to fill/draw with
*/
/**************************************************************************/
void lcdFillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{

    int16_t a, b, y, last;

    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1)
    {
        swap(y0, y1); swap(x0, x1);
    }
    if (y1 > y2)
    {
        swap(y2, y1); swap(x2, x1);
    }
    if (y0 > y1)
    {
        swap(y0, y1); swap(x0, x1);
    }

    if(y0 == y2)
    { // Handle awkward all-on-same-line case as its own thing
        a = b = x0;
        if(x1 < a)      a = x1;
        else if(x1 > b) b = x1;
        if(x2 < a)      a = x2;
        else if(x2 > b) b = x2;
        lcdDrawHLine(a, b + 1, y0, color);
        return;
    }

    int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
    int32_t
    sa   = 0,
    sb   = 0;

    // For upper part of triangle, find scanline crossings for segments
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
    // is included here (and second loop will be skipped, avoiding a /0
    // error there), otherwise scanline y1 is skipped here and handled
    // in the second loop...which also avoids a /0 error here if y0=y1
    // (flat-topped triangle).
    if(y1 == y2) last = y1;   // Include y1 scanline
    else         last = y1-1; // Skip it

    for(y=y0; y<=last; y++)
    {
        a   = x0 + sa / dy01;
        b   = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        /* longhand:
        a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if(a > b) swap(a,b);
        lcdDrawHLine(a, b + 1, y, color);
    }

    // For lower part of triangle, find scanline crossings for segments
    // 0-2 and 1-2.  This loop is skipped if y1=y2.
    sa = (int32_t)dx12 * (y - y1);
    sb = (int32_t)dx02 * (y - y0);
    for(; y<=y2; y++)
    {
        a   = x1 + sa / dy12;
        b   = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        /* longhand:
        a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if(a > b) swap(a,b);
        lcdDrawHLine(a, b + 1, y, color);
    }
}

void lcdDrawImage(uint16_t x, uint16_t y, GUI_CONST_STORAGE GUI_BITMAP* pBitmap)
{
	if((x >= lcdProperties.width) || (y >= lcdProperties.height)) return;
	if((x + pBitmap->xSize - 1) >= lcdProperties.width) return;
	if((y + pBitmap->ySize - 1) >= lcdProperties.height) return;

	for (int i = 0; i < pBitmap->ySize; ++i)
	{
		lcdDrawPixels(x, y + i, (uint16_t*)(pBitmap->pData + i * pBitmap->bytesPerLine), pBitmap->bytesPerLine / (pBitmap->bitsPerPixel / 8));
	}
}

void lcdHome(void)
{
	cursorXY.x = 0;
	cursorXY.y = 0;
	lcdSetWindow(0, 0, lcdProperties.width - 1, lcdProperties.height - 1);
}

/**
 * \brief Draws a character at the specified coordinates
 *
 * \param x			The x-coordinate
 * \param y			The y-coordinate
 * \param c			Character
 * \param color		Character color
 * \param bg		Background color
 * \param size		Character Size
 *
 * \return void
 */
void lcdDrawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg)
{
	if ((x >= lcdProperties.width) || 			// Clip right
			(y >= lcdProperties.height) || 		// Clip bottom
			((x + lcdFont.pFont->Width) < 0) || // Clip left
			((y + lcdFont.pFont->Height) < 0))  // Clip top
		return;

	uint8_t fontCoeff = lcdFont.pFont->Height / 8;
	uint8_t xP = 0;

	for(uint8_t i = 0; i < lcdFont.pFont->Height; i++)
	{
		uint8_t line;

		for(uint8_t k = 0; k < fontCoeff; k++)
		{
			line = lcdFont.pFont->table[((c - 0x20) * lcdFont.pFont->Height * fontCoeff) + (i * fontCoeff) + k];

			for(uint8_t j = 0; j < 8; j++)
			{
				if((line & 0x80) == 0x80)
				{
					lcdDrawPixel(x + j + xP, y + i, color);
				}
				else if (bg != color)
				{
					lcdDrawPixel(x + j + xP, y + i, bg);
				}
				line <<= 1;
			}

			xP += 8;
		}

		xP = 0;
	}
}

/**
 * \brief Print the specified Text
 *
 * \param fmt	Format text
 * \param
 *
 * \return void
 */
void lcdPrintf(const char *fmt, ...)
{
	static char buf[256];
	char *p;
	va_list lst;

	va_start(lst, fmt);
	vsprintf(buf, fmt, lst);
	va_end(lst);

	p = buf;
	while (*p)
	{
		if (*p == '\n')
		{
			cursorXY.y += lcdFont.pFont->Height;
			cursorXY.x = 0;
		}
		else if (*p == '\r')
		{
			// skip em
		}
		else if (*p == '\t')
		{
			cursorXY.x += lcdFont.pFont->Width * 4;
		}
		else
		{
			lcdDrawChar(cursorXY.x, cursorXY.y, *p, lcdFont.TextColor, lcdFont.BackColor);
			cursorXY.x += lcdFont.pFont->Width;
			if (lcdFont.TextWrap && (cursorXY.x > (lcdProperties.width - lcdFont.pFont->Width)))
			{
				cursorXY.y += lcdFont.pFont->Height;
				cursorXY.x = 0;
			}
		}
		p++;

		if (cursorXY.y >= lcdProperties.height)
		{
			cursorXY.y = 0;
		}
	}
}

/**
 * \brief Sets the font
 *
 * \param font pointer font
 *
 * \return void
 */
void lcdSetTextFont(sFONT* font)
{
	lcdFont.pFont = font;
}

/**
 * \brief Sets the text color
 *
 * \param c		Text color
 * \param b		Background color
 *
 * \return void
 */
void lcdSetTextColor(uint16_t c, uint16_t b)
{
	lcdFont.TextColor = c;
	lcdFont.BackColor = b;
}

/**
 * \brief Set Text wrap
 *
 * \param w
 *
 * \return void
 */
void lcdSetTextWrap(uint8_t w)
{
	lcdFont.TextWrap = w;
}

void lcdSetOrientation(lcdOrientationTypeDef value)
{
	lcdProperties.orientation = value;
	lcdWriteCommand(ILI9341_MEMCONTROL);

	switch (lcdProperties.orientation)
	{
		case LCD_ORIENTATION_PORTRAIT:
			lcdWriteData(lcdPortraitConfig);
			lcdProperties.width = ILI9341_PIXEL_WIDTH;
			lcdProperties.height = ILI9341_PIXEL_HEIGHT;
			break;
		case LCD_ORIENTATION_PORTRAIT_MIRROR:
			lcdWriteData(lcdPortraitMirrorConfig);
			lcdProperties.width = ILI9341_PIXEL_WIDTH;
			lcdProperties.height = ILI9341_PIXEL_HEIGHT;
			break;
		case LCD_ORIENTATION_LANDSCAPE:
			lcdWriteData(lcdLandscapeConfig);
			lcdProperties.width = ILI9341_PIXEL_HEIGHT;
			lcdProperties.height = ILI9341_PIXEL_WIDTH;
			break;
		case LCD_ORIENTATION_LANDSCAPE_MIRROR:
			lcdWriteData(lcdLandscapeMirrorConfig);
			lcdProperties.width = ILI9341_PIXEL_HEIGHT;
			lcdProperties.height = ILI9341_PIXEL_WIDTH;
			break;
		default:
			break;
	}

	//lcdWriteCommand(ILI9341_MEMORYWRITE);
	lcdSetWindow(0, 0, lcdProperties.width - 1, lcdProperties.height - 1);
}

void lcdSetCursor(unsigned short x, unsigned short y)
{
	cursorXY.x = x;
	cursorXY.y = y;
	lcdSetWindow(x, y, x, y);
}

/**
 * \brief Sets window address
 *
 * \param x0         Left top window x-coordinate
 * \param y0         Left top window y-coordinate
 * \param x1         Rigth bottom window x-coordinate
 * \param y1         Rigth bottom window y-coordinate
 *
 * \return void
 */
void lcdSetWindow(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1)
{
  lcdWriteCommand(ILI9341_COLADDRSET);
  lcdWriteData((x0 >> 8) & 0xFF);
  lcdWriteData(x0 & 0xFF);
  lcdWriteData((x1 >> 8) & 0xFF);
  lcdWriteData(x1 & 0xFF);
  lcdWriteCommand(ILI9341_PAGEADDRSET);
  lcdWriteData((y0 >> 8) & 0xFF);
  lcdWriteData(y0 & 0xFF);
  lcdWriteData((y1 >> 8) & 0xFF);
  lcdWriteData(y1 & 0xFF);
  lcdWriteCommand(ILI9341_MEMORYWRITE);
}

void lcdBacklightOff(void)
{
	LCD_BL_OFF();
}

void lcdBacklightOn(void)
{
	LCD_BL_ON();
}

void lcdInversionOff(void)
{
	lcdWriteCommand(ILI9341_INVERTOFF);
}

void lcdInversionOn(void)
{
	lcdWriteCommand(ILI9341_INVERTON);
}

void lcdDisplayOff(void)
{
	lcdWriteCommand(ILI9341_DISPLAYOFF);
	LCD_BL_OFF();
}

void lcdDisplayOn(void)
{
	lcdWriteCommand(ILI9341_DISPLAYON);
	LCD_BL_ON();
}

void lcdTearingOff(void)
{
	lcdWriteCommand(ILI9341_TEARINGEFFECTOFF);
}

void lcdTearingOn(bool m)
{
	lcdWriteCommand(ILI9341_TEARINGEFFECTON);
	lcdWriteData(m);
}

uint16_t lcdGetWidth(void)
{
  return lcdProperties.width;
}

uint16_t lcdGetHeight(void)
{
  return lcdProperties.height;
}

uint16_t lcdGetControllerID(void)
{
	uint16_t id;
	lcdWriteCommand(ILI9341_READID4);
	id = lcdReadData();
	id = lcdReadData();
	id = ((uint16_t) lcdReadData() << 8);
	id |= lcdReadData();
	return id;
}

lcdOrientationTypeDef lcdGetOrientation(void)
{
  return lcdProperties.orientation;
}

sFONT* lcdGetTextFont(void)
{
	return lcdFont.pFont;
}

lcdPropertiesTypeDef lcdGetProperties(void)
{
  return lcdProperties;
}

/**
 * \brief Reads a point from the specified coordinates
 *
 * \param x        x-Coordinate
 * \param y        y-Coordinate
 *
 * \return uint16_t     Color
 */
uint16_t lcdReadPixel(uint16_t x, uint16_t y)
{
    uint16_t temp[3];
    // Clip
    if ((x < 0) || (y < 0) || (x >= lcdProperties.width) || (y >= lcdProperties.height))
        return 0;

    lcdWriteCommand(ILI9341_COLADDRSET);
    lcdWriteData((x >> 8) & 0xFF);
    lcdWriteData(x & 0xFF);

    lcdWriteCommand(ILI9341_PAGEADDRSET);
    lcdWriteData((y >> 8) & 0xFF);
    lcdWriteData(y & 0xFF);

    lcdWriteCommand(ILI9341_MEMORYREAD);

    temp[0] = lcdReadData(); // dummy read
    temp[1] = lcdReadData();
    temp[2] = lcdReadData();

    return lcdColor565((temp[1] >> 8) & 0xFF, temp[1] & 0xFF, (temp[2] >> 8) & 0xFF);
}

uint16_t lcdColor565(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/*---------Static functions--------------------------*/

static void lcdDrawPixels(uint16_t x, uint16_t y, uint16_t *data, uint32_t dataLength)
{
  uint32_t i = 0;

  lcdSetWindow(x, y, lcdProperties.width - 1, lcdProperties.height - 1);

  do
  {
    lcdWriteData(data[i++]);
  }
  while (i < dataLength);
}

static void lcdReset(void)
{
	lcdWriteCommand(ILI9341_SOFTRESET);
	HAL_Delay(50);
}

// Write an 8 bit command to the IC driver
static void lcdWriteCommand(unsigned char command)
{
	LCD_CmdWrite(command);
}

// Write an 16 bit data word to the IC driver
static void lcdWriteData(unsigned short data)
{
	LCD_DataWrite(data);
}

static unsigned short lcdReadData(void)
{
	return LCD_DataRead();
}

static unsigned char lcdBuildMemoryAccessControlConfig(
                        bool rowAddressOrder,
                        bool columnAddressOrder,
                        bool rowColumnExchange,
                        bool verticalRefreshOrder,
                        bool colorOrder,
                        bool horizontalRefreshOrder)
{
  unsigned char value 				= 0;
  if(horizontalRefreshOrder) value 	|= ILI9341_MADCTL_MH;
  if(colorOrder) value 				|= ILI9341_MADCTL_BGR;
  if(verticalRefreshOrder) value 	|= ILI9341_MADCTL_ML;
  if(rowColumnExchange) value 		|= ILI9341_MADCTL_MV;
  if(columnAddressOrder) value 		|= ILI9341_MADCTL_MX;
  if(rowAddressOrder) value 		|= ILI9341_MADCTL_MY;
  return value;
}
