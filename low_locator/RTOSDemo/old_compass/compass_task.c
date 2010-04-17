#include "FreeRTOS.h"
#include "compass.h"
#include "compass_task.h"
#include "common.h"
#include "task.h"
#include "pio/pio.h"

#define compassSTACK_SIZE		(200)
#define SPI_PCS(npcs)       ((~(1 << npcs) & 0xF) << 16)

void vCompassTask( void *pvParameters );

/*-----------------------------------------------------------*/

void vStartCompassTask( unsigned portBASE_TYPE uxPriority )
{
unsigned portBASE_TYPE uxLEDTask;

/* Spawn the task. */
 xTaskCreate( vCompassTask,  ( signed portCHAR * ) "Comp", compassSTACK_SIZE, ( void * ) NULL, uxPriority, ( xTaskHandle * ) NULL );
}
/*-----------------------------------------------------------*/

void comp_writecommand(unsigned char command, unsigned char leavenpcslow)
{
  Pin npcs = NPCS3;
  Pin cdin = CDIN;
  Pin cdout = CDOUT;
  Pin spck = SPCK;
  PIO_Clear (&spck);
  int i = 0;
  for (i=0; i<22; i++)
    asm("nop");
  PIO_Clear (&npcs);
  PIO_Set (&spck);
  if ((command & 1<<3))
    PIO_Set (&cdout);
  else 
    PIO_Clear (&cdout);
  for (i=0; i<22; i++)
    asm("nop");
  PIO_Clear (&spck);
  for (i=0; i<22; i++)
    asm("nop");
  if ((command & 1<<2))
    PIO_Set (&cdout);
  else 
    PIO_Clear (&cdout);
  PIO_Set (&spck);
  for (i=0; i<22; i++)
    asm("nop");
  PIO_Clear (&spck);
  for (i=0; i<22; i++)
    asm("nop");
  if ((command & 1<<1))
    PIO_Set (&cdout);
  else 
    PIO_Clear (&cdout);
  PIO_Set (&spck);
  for (i=0; i<22; i++)
    asm("nop");
  PIO_Clear (&spck);
  for (i=0; i<22; i++)
    asm("nop");
  if ((command & 1))
    PIO_Set (&cdout);
  else 
    PIO_Clear (&cdout);
  PIO_Set (&spck);
  for (i=0; i<10; i++)
    asm("nop");
  if (leavenpcslow==0)
    PIO_Set (&npcs);
}

void comp_readdata(unsigned char leavenpcslow)
{
  Pin npcs3 = NPCS3;
  Pin spck = SPCK;
  Pin cdin = CDIN;
  int i = 0;
  cmpstat.currentoffset = 0;
  cmpstat.header = 0;
  while (cmpstat.currentoffset < 4)
    {
      cmpstat.header |= PIO_Get(&cdin)<<cmpstat.currentoffset;
      cmpstat.currentoffset += 1;
      PIO_Clear (&spck);
      for (i=0; i<22; i++)
	asm("nop");

      PIO_Set (&spck);
      for (i=0; i<22; i++)
	asm("nop");
    }
  if (leavenpcslow==0)
    PIO_Set (&npcs3);
}

void comp_readaxis(unsigned char leavenpcslow)
{
  Pin spck = SPCK;
  Pin cdin = CDIN;
  Pin npcs3 = NPCS3;
  int i = 0;
  cmpstat.currentoffset = 0;
  cmpstat.data = 0;
  while (cmpstat.currentoffset < 22)
    {
      cmpstat.data |= PIO_Get(&cdin)<<(cmpstat.currentoffset);
      cmpstat.currentoffset += 1;
      PIO_Clear (&spck);
      for (i=0; i<22; i++)
	asm("nop");

      PIO_Set (&spck);
      for (i=0; i<22; i++)
	asm("nop");
    }
  if (leavenpcslow==0)
    PIO_Set (&npcs3);
}

void vCompassTask( void *pvParameters )
{
  int i = 0;
  Pin npcs = NPCS3;
  trspistat.cmpp.compassstat = 0;
  cmpstat.header = 0;
  cmpstat.currentoffset = 0;
  extern void ( vSPI_ISR_Wrapper )( void );
  iostat.portchange = 0;
  //  AT91C_BASE_PIOA->PIO_IER = 1<<12;
  //  AIC_ConfigureIT(AT91C_ID_PIOA, 0, vSPI_ISR_Wrapper);
  //  AIC_EnableIT(AT91C_ID_PIOA);

  unsigned char command = 0x0;

  trspistat.cmpp.compassstat = 1;
  comp_writecommand(command, 0);
  vTaskDelay( 50 / portTICK_RATE_MS );
  trspistat.cmpp.compassstat = 2;
  command = 0x8;
  comp_writecommand(command, 0);
  vTaskDelay( 50 / portTICK_RATE_MS );
  /*PIO_Clear (&npcs);
  for (i=0; i<22; i++)
    asm("nop");
  PIO_Set (&npcs);
  for (i=0; i<22; i++)
    asm("nop");
  PIO_Clear (&npcs);
  for (i=0; i<22; i++)
  asm("nop");*/
  trspistat.cmpp.compassstat = 3;
  command = 0xC;
  comp_writecommand(command, 1);
  trspistat.cmpp.compassstat = 4;
  comp_readdata(0);
  trspistat.cmpp.compassstat = cmpstat.header;

  for(;;)
    {
      /*if (cmpstat.header == 15)
	{*/
	  command = 0x0;
	  comp_writecommand(command, 0);
	  vTaskDelay( 10 / portTICK_RATE_MS );
	  command = 0x8;
	  comp_writecommand(command, 0);
	  /*}
      else if (cmpstat.header == 12)
	{
	  vTaskDelay( 50 / portTICK_RATE_MS );
	  comp_readdata(3, 0);
	  }*/

      vTaskDelay( 50 / portTICK_RATE_MS );
      command = 0xC;
      comp_writecommand(command, 1);
      //vTaskDelay( 50 / portTICK_RATE_MS );
      comp_readdata(1);
      comp_readaxis(0);
      trspistat.cmpp.compassstat = cmpstat.header;
      trspistat.cmpp.compassdata = cmpstat.data;
      /*      command = 0x8;
      comp_writecommand(command, 3, 1);

      while (cmpstat.header != 3)
	{
	  vTaskDelay( 50 / portTICK_RATE_MS );
	  PIO_Set (&npcs);
	  for (i=0; i<22; i++)
	    asm("nop");
	  PIO_Clear (&npcs);
	  for (i=0; i<22; i++)
	  asm("nop");
	  command = 0xC;
	  comp_writecommand(command, 3, 0);
	  comp_readdata(3, 0);
	  trspistat.compassstat = cmpstat.header;
	  }*/

      /* Delay */
    }
}
