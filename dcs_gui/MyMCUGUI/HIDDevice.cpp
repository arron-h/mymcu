#include "HIDDevice.h"
#include <stdio.h>


namespace
{
	const unsigned char MYMCU_HEADER_A = 0xFC;
	const unsigned char MYMCU_HEADER_B = 0xAB;

	const unsigned int MYMCU_VID = 0x16C0;
	const unsigned int MYMCU_PID = 0x0482;

	const unsigned int MYMCU_MAX_MSG_SIZE = 32;

}

HIDDevice::HIDDevice() : m_handle(NULL)
{
	int res;
	res = hid_init();
	if (res)
	{
		printf("Failed to initialise hidapi\n");
	}
}

HIDDevice::~HIDDevice()
{
	hid_exit();
}

bool HIDDevice::Open()
{
	m_handle = hid_open(0x16C0, 0x0482, NULL);
	if (m_handle == NULL)
	{
		printf("Failed to open HID device. Is it plugged in?\n");
		return false;
	}

	wchar_t wstr[256];
	hid_get_product_string(m_handle, wstr, 256);
	wprintf(L"Found matching HID device with product string: %s\n", wstr);

	hid_set_nonblocking(m_handle, 1);

	return true;
}

void HIDDevice::Close()
{
	hid_close(m_handle);
}

void HIDDevice::Write(unsigned char cmd, unsigned char value)
{
	std::vector<unsigned char> data;
	data.push_back(cmd);
	data.push_back(value);
	Write(data);
}

void HIDDevice::Write(std::vector<unsigned char> data)
{
	if (m_handle == NULL)
		return;

	std::vector<unsigned char> buf;
	buf.push_back(0x0);            // Report ID
	buf.push_back(MYMCU_HEADER_A); // Header A
	buf.push_back(MYMCU_HEADER_B); // Header B
	buf.insert(buf.begin() + 3, data.begin(), data.end()); // Following data

	int bytesWritten = hid_write(m_handle, buf.data(), buf.size());
	if (bytesWritten == -1)
	{
		const wchar_t* lastError = hid_error(m_handle);
		wprintf(L"HID error: Failed to write %llu bytes. Reason: %s\n", buf.size(), lastError);
	}
}

std::vector<unsigned char> HIDDevice::Read(int numBytes, bool retainMetadata)
{
	std::vector<unsigned char> buf;
	std::vector<unsigned char> ret;

	if (m_handle == NULL)
		return buf;

	buf.resize(numBytes + 3);

	int bytesRead = hid_read(m_handle, buf.data(), buf.size());
	if (bytesRead == -1)
	{
		const wchar_t* lastError = hid_error(m_handle);
		wprintf(L"HID error: Failed to read %llu bytes. Reason: %s\n", buf.size(), lastError);
	}
	else if (bytesRead > 0)
	{
		if (buf[0] == MYMCU_HEADER_A && buf[1] == MYMCU_HEADER_B)
		{
			ret.assign(retainMetadata ? buf.begin() : buf.begin() + 2, buf.end());
		}
	}

	return ret;
}