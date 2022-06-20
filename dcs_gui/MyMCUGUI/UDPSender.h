#pragma once
#include <vector>

class UDPSenderImpl;
class UDPSender
{
public:
	UDPSender();
	~UDPSender();

	int Init();
	int Send(std::vector<unsigned char>& data);

private:
	UDPSenderImpl* impl;
};

