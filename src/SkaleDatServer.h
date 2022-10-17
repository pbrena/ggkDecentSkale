// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is the top-level interface for the Scale full description and implementation.
//
// >>
// >>>  DISCUSSION
// >>
//
// See the discussion at the top of SkaleCntrl.cpp
//
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifndef SkaleDatServer
#define SkaleDatServer

#pragma once

#include <stdint.h>
#include <thread>
#include <mutex>


//#include "HciSocket.h"
//#include "Logger.h"

#include <random>
class Skale
{
public:

	//
	// Variables
	//
	
	// Used or sake of Thread-safety <<<---
	static std::mutex SkaleMutex;

	static int16_t PesoRaw;      // Grams * 10
	static int16_t PesoRawAntes;
	static int16_t PesoConTara;
	static int16_t OffsetPaTara;
	static int16_t DiferenciaPeso;
	static uint8_t WeightStable;
	static bool    LedOn;
	static bool    GramsOn;
	static bool    TimerOn;
	static std::vector<uint8_t>   WeightReport; // 03=Decent Type CE=weightstable 0000=weight 0000=Change =XorValidation
	//
	// Constans
	//
	// Wait time before new scale values update cycle
	static const int16_t kRescanTimeMS = 97;
	//static constexpr std::chrono::milliseconds kRescanTimeMS = std::chrono::milliseconds(97);
	//static constexpr std::chrono::milliseconds kRescanTimeMS = std::chrono::milliseconds(kRescanTimeMS);
	//
	// Methods
	//
	// Returns the instance to this singleton class
	// Al llamar 1a vez creala instancia???
	static Skale &getInstance()
	{
		static Skale instance;
		return instance;
	}

	//
	// Disallow copies of our singleton (c++11)
	//
	Skale(Skale const&) = delete;
	void operator=(Skale const&) = delete;

	// If the thread is already running, this method will fail
	// Note that it shouldn't be necessary to connect manually; 
	// Returns true if the Skale thread initiates otherwise false
	static bool start();
	// This method will block until the SkaleCont thread joins
	static void stop();

	static std::vector<uint8_t>   SkaleResponce();
	static bool        SkaleProcKmd(std::vector<uint8_t>   SkaleKmd);

	// Our thread interface, which simply launches our the thread processor on our Skale instance
	// This mehtod should not be called directly. Rather, it is started with start/stop methods
	static void runContThread();

private:

	// Private constructor for our Singleton force use of getInstanc
	Skale() { };
	//
	// Constants
	//
	// Our continous thread listens for events coming from the adapter and deals with them appropriately
	static std::thread contThread;

	static const int  peso  = 2;   
	static const int  difer = 4; 

	static const char kSkaleStable    = '\xCE'; // weight stable
	static const char kSkaleChning    = '\xCA'; // weight changing
	static const char kSkaleLEDnGrKmd = '\x0A'; // LED and grams on/off
	static const char kSkaleTimerKmd  = '\x0B'; // Timer on/off
	static const char kSkaleTareKmd   = '\x0F'; // UtilTare

	//
	// Methods
	//
	static void    UtilInserta(int Cual, int16_t Valor, std::vector<uint8_t>   Mensaje);
	static void    UtilTare();
	static int16_t UtilLeePesoHW();

};
#endif