#pragma once

#include <functional>
#include <vector>

class UDPMulticastReceiverImpl;
class UDPMulticastReceiver
{
public:
	UDPMulticastReceiver();
	~UDPMulticastReceiver();

	int Init();
	int Recv(std::function<void(std::vector<char>&)> dataFunc);

private:
	UDPMulticastReceiverImpl* impl;
};

