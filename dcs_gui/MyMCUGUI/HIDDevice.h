#pragma once

#include <vector>
#include "hidapi/hidapi.h"

class HIDDevice
{
public:
	enum Commands
	{
		CMDS_ANNUC_CHANGED = 0x1,
		CMDS_COM_ACT_CHANGED = 0x2,
		CMDS_COM_SBY_CHANGED = 0x3,
		CMDS_NAV_ACT_CHANGED = 0x4,
		CMDS_NAV_SBY_CHANGED = 0x5,
		CMDS_LCD_ON = 0x6,
		CMDS_LCD_OFF = 0x7,
		CMDS_HEARTBEAT = 0x8,
		CMDS_LCD_STRING = 0x9,
		CMDS_ROTARY_CHANGED = 0xA
	};

	enum Annunciator
	{
		ANNUC_CAUT_ON = 0x1,
		ANNUC_CAUT_OFF = 0x2,
		ANNUC_WARN_ON = 0x4,
		ANNUC_WARN_OFF = 0x8,
		ANNUC_AP_ON = 0x10,
		ANNUC_AP_OFF = 0x20,
		ANNUC_YD_ON = 0x40,
		ANNUC_YD_OFF = 0x80,
	};

	enum RotaryDirection
	{
		ROTARYDIR_INC = 0,
		ROTARYDIR_DEC = 1,
	};

	enum Rotaries
	{
		ROTARY_COM_INNER = 1,
		ROTARY_COM_OUTER = 2,
		ROTARY_NAV_INNER = 3,
		ROTARY_NAV_OUTER = 4,
		ROTARY_FMS_INNER = 5,
		ROTARY_FMS_OUTER = 6,
		ROTARY_HDG = 7,
		ROTARY_CRS = 8,
		ROTARY_ALT = 9,
		ROTARY_UPDN = 10,
	};

public:
	HIDDevice();
	~HIDDevice();

	bool Open();
	void Close();

	void Write(unsigned char cmd, unsigned char value);
	void Write(std::vector<unsigned char> data);
	std::vector<unsigned char> Read(int numBytes, bool retainMetadata);

private:
	hid_device* m_handle;
};
