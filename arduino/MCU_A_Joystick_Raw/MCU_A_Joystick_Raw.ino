#include <usb_desc.h>
#include "SingleRotary.h"

/* Pin definitions */
// LEDs
const int warningLedPin = 27;
const int cautionLedPin = 26;
const int apLedPin      = 24;
const int ydLedPin      = 25;
// Rotaries
const int rotaryCOMOuterPinA = 2;
const int rotaryCOMOuterPinB = 3;
const int rotaryCOMInnerPinA = 4;
const int rotaryCOMInnerPinB = 5;
const int rotaryNAVOuterPinA = 6;
const int rotaryNAVOuterPinB = 7;
const int rotaryNAVInnerPinA = 8;
const int rotaryNAVInnerPinB = 9;
const int rotaryFMSOuterPinA = 10;
const int rotaryFMSOuterPinB = 11;
const int rotaryFMSInnerPinA = 12;
const int rotaryFMSInnerPinB = 14;
const int rotaryHDGPinA  = 15;
const int rotaryHDGPinB  = 16;
const int rotaryCRSPinA  = 17;
const int rotaryCRSPinB  = 18;
const int rotaryALTPinA  = 19;
const int rotaryALTPinB  = 20;
const int rotaryUPDNPinA = 21;
const int rotaryUPDNPinB = 22;

/* HID parameters */
byte hidDataBufferRx[RAWHID_RX_SIZE];
byte hidDataBufferTx[RAWHID_TX_SIZE];

/* Encoder setup */
enum MyMCURotaryIds
{
	MYMCU_ROTARY_COM_INNER = 0x1,
	MYMCU_ROTARY_COM_OUTER,
	MYMCU_ROTARY_NAV_INNER,
	MYMCU_ROTARY_NAV_OUTER,
	MYMCU_ROTARY_FMS_INNER,
	MYMCU_ROTARY_FMS_OUTER,
	MYMCU_ROTARY_HDG,
	MYMCU_ROTARY_CRS,
	MYMCU_ROTARY_ALT,
	MYMCU_ROTARY_UPDN,
	
	MYMCU_NUM_ROTARIES = MYMCU_ROTARY_UPDN
};
SingleRotary rotaries[MYMCU_NUM_ROTARIES] = 
{
	SingleRotary(rotaryCOMOuterPinA, rotaryCOMOuterPinB, MYMCU_ROTARY_COM_OUTER, true,  SingleRotary::RATIO_21),
	SingleRotary(rotaryCOMInnerPinA, rotaryCOMInnerPinB, MYMCU_ROTARY_COM_INNER, true,  SingleRotary::RATIO_21),
	SingleRotary(rotaryNAVOuterPinA, rotaryNAVOuterPinB, MYMCU_ROTARY_NAV_OUTER, false, SingleRotary::RATIO_21),
	SingleRotary(rotaryNAVInnerPinA, rotaryNAVInnerPinB, MYMCU_ROTARY_NAV_INNER, true,  SingleRotary::RATIO_21),
	SingleRotary(rotaryFMSOuterPinA, rotaryFMSOuterPinB, MYMCU_ROTARY_FMS_OUTER, false, SingleRotary::RATIO_21),
	SingleRotary(rotaryFMSInnerPinA, rotaryFMSInnerPinB, MYMCU_ROTARY_FMS_INNER, false, SingleRotary::RATIO_21),
	SingleRotary(rotaryHDGPinA,      rotaryHDGPinB,      MYMCU_ROTARY_HDG,       false, SingleRotary::RATIO_11),
	SingleRotary(rotaryCRSPinA,      rotaryCRSPinB,      MYMCU_ROTARY_CRS,       false, SingleRotary::RATIO_11),
	SingleRotary(rotaryALTPinA,      rotaryALTPinB,      MYMCU_ROTARY_ALT,       false, SingleRotary::RATIO_11),
	SingleRotary(rotaryUPDNPinA,     rotaryUPDNPinB,     MYMCU_ROTARY_UPDN,      true,  SingleRotary::RATIO_11)
};

/* MyMCU Message Protol */
const byte MYMCU_HEADER[2] = {0xFC,0xAB};
enum MyMcuMessageCommands
{
	MYMCU_CMDS_ANNUC_CHANGED = 0x1,
	MYMCU_CMDS_COM_ACT_CHANGED,
	MYMCU_CMDS_COM_SBY_CHANGED,
	MYMCU_CMDS_NAV_ACT_CHANGED,
	MYMCU_CMDS_NAV_SBY_CHANGED,
	MYMCU_CMDS_LCD_ON,
	MYMCU_CMDS_LCD_OFF,
	MYMCU_CMDS_LCD_HEARTBEAT,
	MYMCU_CMDS_LCD_STRING,
	MYMCU_CMDS_ROTARY_CHANGED,

	MYMCU_CMDS_MAX,
	MYMCU_CMDS_LCD_OFFSET = MYMCU_CMDS_COM_ACT_CHANGED
};

enum MyMcuAnnuciatorValues
{
	MYMCU_ANNUC_CAUT_ON  = 0x1,
	MYMCU_ANNUC_CAUT_OFF = 0x2,
	MYMCU_ANNUC_WARN_ON  = 0x4,
	MYMCU_ANNUC_WARN_OFF = 0x8,
	MYMCU_ANNUC_AP_ON    = 0x10,
	MYMCU_ANNUC_AP_OFF   = 0x20,
	MYMCU_ANNUC_YD_ON    = 0x40,
	MYMCU_ANNUC_YD_OFF   = 0x80,
};

unsigned long startMs; 
unsigned long lastHeartbeat;
boolean startupRan = false;

void processAnnunciator(byte value)
{
	switch(value)
	{
	case MYMCU_ANNUC_CAUT_ON:
		digitalWrite(cautionLedPin, HIGH);
		break;
	case MYMCU_ANNUC_CAUT_OFF:
		digitalWrite(cautionLedPin, LOW);
		break;
	case MYMCU_ANNUC_WARN_ON:
		digitalWrite(warningLedPin, HIGH);
		break;
	case MYMCU_ANNUC_WARN_OFF:
		digitalWrite(warningLedPin, LOW);
		break;
	case MYMCU_ANNUC_AP_ON:
		digitalWrite(apLedPin, HIGH);
		break;
	case MYMCU_ANNUC_AP_OFF:
		digitalWrite(apLedPin, LOW);
		break;
	case MYMCU_ANNUC_YD_ON:
		digitalWrite(ydLedPin, HIGH);
		break;
	case MYMCU_ANNUC_YD_OFF:
		digitalWrite(ydLedPin, LOW);
		break;
	}
}

void LedControl(int val)
{
	digitalWrite(cautionLedPin, val);
	digitalWrite(warningLedPin, val);
	digitalWrite(apLedPin, val);
	digitalWrite(ydLedPin, val);
	digitalWrite(13, val);
}

void allLedsOn()
{
	LedControl(HIGH);
}

void allLedsOff()
{
	LedControl(LOW);
}

void setup()
{
	Serial.begin(9600);

	// Set LED pins
	pinMode(cautionLedPin, OUTPUT);
	pinMode(warningLedPin, OUTPUT);
	pinMode(apLedPin, OUTPUT);
	pinMode(ydLedPin, OUTPUT);
	pinMode(13, OUTPUT); // Board LED pin

	// Put ALL LEDs ON initially
	allLedsOn();

	// Calculate start time
	startMs = millis();
	lastHeartbeat  = startMs;

	hidDataBufferTx[0] = MYMCU_HEADER[0];
	hidDataBufferTx[1] = MYMCU_HEADER[1];
}

void loop()
{
	unsigned long msNow = millis();

	/*
	* Illuminate all LEDs when we power up for 0.5s
	*/
	if (!startupRan)
	{
		if (msNow - startMs > 500)
		{
			allLedsOff();
			startupRan = true;
		}
	}

	/*
	* Process rotaries
	*/
	for (int i = 0; i < MYMCU_NUM_ROTARIES; ++i)
	{
		SingleRotary& rotary = rotaries[i];
		
		rotary.Process();
		
		if (rotary.Available())
		{
			hidDataBufferTx[2] = (uint8_t)MYMCU_CMDS_ROTARY_CHANGED;
			hidDataBufferTx[3] = rotary.Id();                // Rotary ID
			hidDataBufferTx[4] = rotary.Direction();         // Rotary direction
			hidDataBufferTx[5] = rotary.DetentsPerFrame();   // Rotary detents per frame
			RawHID.send(hidDataBufferTx, 0);
			rotary.Reset();
		}
	}

	/*
	* Read HID data
	*/
	int bytesRead = RawHID.recv(hidDataBufferRx, 0);
	if (bytesRead >= 3)
	{
		if (hidDataBufferRx[0] == MYMCU_HEADER[0] && hidDataBufferRx[1] == MYMCU_HEADER[1])
		{
			byte cmd = hidDataBufferRx[2];
			switch(cmd)
			{
				case MYMCU_CMDS_ANNUC_CHANGED:
					processAnnunciator(hidDataBufferRx[3]);
					break;
				case MYMCU_CMDS_LCD_HEARTBEAT:
					lastHeartbeat = millis();
					break;
				// Legacy commands - do not handle
				case MYMCU_CMDS_COM_ACT_CHANGED:
				case MYMCU_CMDS_COM_SBY_CHANGED:
				case MYMCU_CMDS_NAV_ACT_CHANGED:
				case MYMCU_CMDS_NAV_SBY_CHANGED:
				case MYMCU_CMDS_LCD_ON:
				case MYMCU_CMDS_LCD_OFF:
				case MYMCU_CMDS_LCD_STRING:
				default:
					break;
			}
		}
	}
}
