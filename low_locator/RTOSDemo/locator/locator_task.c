#include "FreeRTOS.h"
#include "locator.h"
#include "locator_task.h"
#include "common.h"
#include "task.h"
#include "pio/pio.h"
#include "fft/fix_fft.h"

#define locatorSTACK_SIZE		(1024)
#define SPI_PCS(npcs)       ((~(1 << npcs) & 0xF) << 16)

void vLocatorTask( void *pvParameters );

/*-----------------------------------------------------------*/

void vStartLocatorTask( unsigned portBASE_TYPE uxPriority )
{
/* Spawn the task. */
 xTaskCreate( vLocatorTask,  ( signed portCHAR * ) "Loc", locatorSTACK_SIZE, ( void * ) NULL, uxPriority, ( xTaskHandle * ) NULL );
}
/*-----------------------------------------------------------*/

void loc_writecommand(unsigned char command, unsigned int snpcs, unsigned char leavenpcslow)
{
  Pin npcs = NPCS_LOC;
  Pin cdout = CDOUT_LOC;
  Pin spck = SPCK_LOC;
  portDISABLE_INTERRUPTS();
  PIO_Clear (&spck);
  int i = 0;
  int j = 0;
  for (i=0; i<100; i++)
    asm("nop");
  PIO_Clear (&npcs);
  for (i=0; i<100; i++)
    asm("nop");
  for (j=0; j<8; j++)
    {
      PIO_Set (&spck);
      if ((command & 1<<(7-j)))
	PIO_Set (&cdout);
      else 
	PIO_Clear (&cdout);
      for (i=0; i<100; i++)
	asm("nop");
      PIO_Clear (&spck);
      for (i=0; i<100; i++)
	asm("nop");
    }
  PIO_Set (&spck);
  for (i=0; i<100; i++)
    asm("nop");
if (leavenpcslow==0)
    PIO_Set (&npcs);
 portENABLE_INTERRUPTS();
}

void vLocatorTask( void *pvParameters )
{
  extern void ( vLOC_ISR_Wrapper )( void );
  extern void ( vLOC_TC_ISR_Wrapper )( void );

  struct complex in[FFT_SIZE];

  unsigned int id_channel;
  trspistat.channels[0].channel = ADC_CHANNEL_0;
  trspistat.channels[1].channel = ADC_CHANNEL_1;
  trspistat.channels[2].channel = ADC_CHANNEL_2;
  trspistat.channels[3].channel = ADC_CHANNEL_3;
  trspistat.channels[4].channel = ADC_CHANNEL_4;
  trspistat.channels[5].channel = ADC_CHANNEL_5;
  trspistat.channels[6].channel = ADC_CHANNEL_6;
  trspistat.channels[7].channel = ADC_CHANNEL_7;
  trspistat.processed = 1;
  

  int i = 0;
  Pin npcs = NPCS_LOC;

  unsigned char command = 0x0;

  //  trspistat.compassstat = 1;

  //------------------------------------------------------------------------------
  /// Configures Timer Counter 0 (TC0) to generate an interrupt every second. This
  /// interrupt will be used to display the number of bytes received on the USART.
  //------------------------------------------------------------------------------
  unsigned int div, tcclks;
  
  // Enable TC0 peripheral clock
  
  if ((AT91C_BASE_PMC->PMC_PCSR & (1 << AT91C_ID_TC0)) == (1 << AT91C_ID_TC0)) 
    {
      
    }
  else 
    { 
      AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_TC0;
    }
  
  ADC_Initialize( AT91C_BASE_ADC,
		  AT91C_ID_ADC,
		  AT91C_ADC_TRGEN_DIS,
		  0,
		  AT91C_ADC_SLEEP_NORMAL_MODE,
		  AT91C_ADC_LOWRES_10_BIT,
		  BOARD_MCK,
		  BOARD_ADC_FREQ,
		  20,
		  800);
  

  ADC_EnableChannel(AT91C_BASE_ADC, ADC_CHANNEL_0);
  ADC_EnableChannel(AT91C_BASE_ADC, ADC_CHANNEL_1);
  ADC_EnableChannel(AT91C_BASE_ADC, ADC_CHANNEL_2);
  ADC_EnableChannel(AT91C_BASE_ADC, ADC_CHANNEL_3);
  ADC_EnableChannel(AT91C_BASE_ADC, ADC_CHANNEL_4);
  ADC_EnableChannel(AT91C_BASE_ADC, ADC_CHANNEL_5);
  ADC_EnableChannel(AT91C_BASE_ADC, ADC_CHANNEL_6);
  ADC_EnableChannel(AT91C_BASE_ADC, ADC_CHANNEL_7);

  AIC_ConfigureIT(AT91C_ID_ADC, 0, vLOC_ISR_Wrapper);

  AIC_EnableIT(AT91C_ID_ADC);

  ADC_EnableIt(AT91C_BASE_ADC,ADC_CHANNEL_0);
  // Start measurement
  ADC_StartConversion(AT91C_BASE_ADC);

  /// Configure TC for a 1s (= 1Hz) tick
  /*  if (TC_FindMckDivisor(390625, BOARD_MCK, &div, &tcclks)==1)
      {*/
  tcclks = 2;
  div = 4;
  TC_Configure(AT91C_BASE_TC0, tcclks | AT91C_TC_CPCTRG);
  AT91C_BASE_TC0->TC_RC = div;//(BOARD_MCK / (2 * div));
  
  // Configure interrupt on RC compare
  AIC_ConfigureIT(AT91C_ID_TC0, 0, vLOC_TC_ISR_Wrapper);
  AT91C_BASE_TC0->TC_IER = AT91C_TC_CPCS;
  AIC_EnableIT(AT91C_ID_TC0);
  TC_Start(AT91C_BASE_TC0);
      //    }
  
  
  // Start measurement
  unsigned char chan = 0;
  unsigned short max = 0;
  //short x[N];
  int z = 0;
  unsigned long timebefore = 0;
  trspistat.timeafter = 0;
  InitFFTTables();
  trspistat.counter=0;
		  
  int octr = 0;
  for(;;)
    {
      if (trspistat.padatachange == 1)
	{
	  switch (trspistat.paen)
	    {
	    case 1:
	      PIO_Configure(pa7en, PIO_LISTSIZE(pa7en));
	      break;
	    default:
	      PIO_Configure(pa7dis, PIO_LISTSIZE(pa7dis));
	      break;
	    }
	  trspistat.padatachange = 0;
	}
      if (octr%10 == 0)
	{
	  if (trspistat.leds[0].state == 1)
	    {
	      trspistat.leds[0].state = 0;
	      trspistat.leds[0].changed = 1;
	    }
	  else
	    {
	      trspistat.leds[0].state = 1;
	      trspistat.leds[0].changed = 1;
	    }
	}
      vTaskDelay(5 / portTICK_RATE_MS );
      for (i = 0;i<LOC_NUMADCCHANNELS; i++)
	{
	  if (trspistat.channels[i].ampchanged == 1)
	    {
	      if (i==5)
		{
		  loc_writecommand(trspistat.channels[i].amp, 3, 0);
		  trspistat.channels[i].ampchanged =0;
		}
	    }
	}
      if ( trspistat.processed == 0)
	{
	  timebefore = xTaskGetTickCount();	  
	  for (i = 0;i<LOC_NUMADCCHANNELS; i++)
	    {
	      trspistat.channels[i].freqamount = 0;
	      for (z=0;z<LOC_NUMSAMPLES;z++)
		{
		  if (trspistat.channels[i].adcbuf[z]-MIDDLEPOINT>max)
		    max = trspistat.channels[i].adcbuf[z]-MIDDLEPOINT;
		  in[z].r = trspistat.channels[i].adcbuf[z]-MIDDLEPOINT;
		  in[z].i=0;
		}
	      DoFFT(in, FFT_SIZE);
	      for (z=0;z<LOC_NUMSAMPLES;z++)
		{
		  if (z>LEFT_BORDER && z<RIGHT_BORDER)
		    {
		      trspistat.channels[i].freqamount += abs(in[z].r);
		    }
		  trspistat.channels[i].fx[z] = in[z].r;
		}
	      
	      if (max>ALLOWED_MAX)
		{
		  if (trspistat.channels[i].amp>0)
		    {
		      trspistat.channels[i].amp = trspistat.channels[i].amp - 1;
		      trspistat.channels[i].ampchanged = 1;
		    }
		}
	      else if (max<ALLOWED_MIN)
		{
		  if (trspistat.channels[i].amp<15)
		    {
		      trspistat.channels[i].amp = trspistat.channels[i].amp + 1;
		      trspistat.channels[i].ampchanged = 1;
		    }
		}
	      max = 0;
	    }
	  trspistat.timeafter = xTaskGetTickCount();
	  trspistat.counter = trspistat.timeafter - timebefore;
	  trspistat.usbdataready = 1;
	  while (trspistat.usbdataready == 1)
	    {
	      vTaskDelay(1 / portTICK_RATE_MS );
	    }
	  octr ++;
	  trspistat.usbdataready = 0;
	  trspistat.channelconverted = 0;
	  trspistat.processed = 1;
	  ADC_EnableChannel(AT91C_BASE_ADC, trspistat.channelconverted);
	  ADC_EnableIt(AT91C_BASE_ADC, trspistat.channelconverted);
	  ADC_StartConversion(AT91C_BASE_ADC);
	  TC_Start(AT91C_BASE_TC0);
	}
    }
}
