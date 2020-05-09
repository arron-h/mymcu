#include "SingleRotary.h"

#define DEBOUNCE_TIME 6U
#define ROTARY_FRAME_TIME 60U

namespace
{
	SingleRotary::EncoderDirection calculateEncoderDirection(int32_t newPos, int32_t lastPos)
	{
		if (newPos > lastPos)
			return SingleRotary::ENCODERDIR_INC;
		else if (newPos < lastPos)
			return SingleRotary::ENCODERDIR_DEC;

		return SingleRotary::ENCODERDIR_STATIC;
	}
}

SingleRotary::SingleRotary(const int pinA, const int pinB, byte id, bool flip, Ratio ratio)
	: enc(pinA, pinB),
	lastPosition(999),
	lastDirection(ENCODERDIR_INVALID),
	flipDir(flip),
    detentPerTurn(ratio)
{
	this->id = id;
	lastMs = millis();
	frameMs = millis();
	
	Reset();
}

void SingleRotary::Process()
{
	unsigned long msNow = millis();
	
	if (msNow - lastMs > DEBOUNCE_TIME)
	{
		int32_t newPosition = enc.read();
		if (newPosition != lastPosition)
		{
			EncoderDirection newDirection = calculateEncoderDirection(newPosition, lastPosition);
			if (newDirection != lastDirection)
			{
				detentsPerFrame = 0;
			}
			detentsPerFrame++;

			lastDirection = newDirection;
			lastPosition  = newPosition;
			lastMs        = msNow;
		}
	}
}

bool SingleRotary::Available()
{
	unsigned long msNow = millis();
	
	if (msNow - frameMs > ROTARY_FRAME_TIME)
	{
		frameMs = msNow;
		return detentsPerFrame > 0 ? true : false;
	}
	
	return false;
}

void SingleRotary::Reset()
{
	detentsPerFrame = 0;
}

byte SingleRotary::Direction()
{
	byte dir = (byte)lastDirection;
	if (flipDir)
	{
		dir = (dir == ENCODERDIR_INC ? ENCODERDIR_DEC : ENCODERDIR_INC);
	}
	return dir;
}

byte SingleRotary::DetentsPerFrame()
{
	if (detentPerTurn == RATIO_11)
	{
		return (byte)detentsPerFrame;
	}
	else if (detentPerTurn == RATIO_21)
	{
		// Sometimes I've seen odd number of detents. Bounce maybe?
		return (detentsPerFrame / 2) + detentsPerFrame % 2;
	}
}
