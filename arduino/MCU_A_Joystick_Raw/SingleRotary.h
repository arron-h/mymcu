#ifndef SINGLE_ROTARY_H
#define SINGLE_ROTARY_H

#include <Arduino.h>
#include <Encoder.h>

class SingleRotary
{
public:
	enum EncoderDirection
	{
		ENCODERDIR_INC,
		ENCODERDIR_DEC,
		ENCODERDIR_STATIC,

		ENCODERDIR_INVALID
	};

	enum Ratio
	{
		RATIO_11,
		RATIO_21
	};
	
public:
	SingleRotary(const int pinA, const int pinB, byte id, bool flip, Ratio ratio);
	
	void Process();
	bool Available();
	void Reset();
	
	byte Id() { return id; }
	byte Direction();
	byte DetentsPerFrame();
	
private:
	Encoder 		   enc;
  int32_t            pulseCount;
	unsigned long 	   lastMs;
	unsigned long      frameMs;
	int32_t			   lastPosition;
	EncoderDirection   lastDirection;
	int32_t            detentsPerFrame;              
	byte 		       id;
    bool               flipDir;
    Ratio              detentPerTurn;
};

#endif
