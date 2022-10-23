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

#include <glib.h> // Para acceder a GVariant
#include <random> // Temporal pa generar simulacion de Peso

class Skale
{
public:

	//
	// Variables
	//
	
	// Used or sake of Thread-safety <<<---
	static std::mutex SkaleMutex;

	static bool    BanderaCiclo;  // bandera para terminar ciclo continuo
	static int16_t PesoRaw;      // Grams * 10
	static int16_t PesoConTara;
	static int16_t OffsetPaTara;
	static int16_t DiferenciaPeso;
	static uint8_t WeightStable;
	static bool    LedOn;
	static bool    GramsOn;
	static bool    TimerOn;
	static std::vector<guint8> MessagePacket; // 03=Decent Type CE=weightstable xxxx=weight xxxx=Change =XorValidation
	//
	// Constans
	//
	// Wait time before new scale values update cycle
	static const int16_t kRescanTimeMS = 100;

	//
	// Methods
	//
	// Returns the instance to this singleton class
	// Al llamar 1a vez crea la instancia???
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
	static bool stop();

	static std::vector<guint8> CurrentPacket();
	static bool SkaleProcKmd(const std::vector<guint8> &SkaleKmd);

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

	static const guint8  peso  = 2;   // Posicion del Peso
	static const guint8  difer = 4;   // Posicion de la diferencia

	static const guint8 kSkaleStable    = 0xCE; // weight stable
	static const guint8 kSkaleChning    = 0xCA; // weight changing
	static const guint8 kSkaleLEDnGrKmd = 0x0A; // LED and grams on/off
	static const guint8 kSkaleTimerKmd  = 0x0B; // Timer on/off
	static const guint8 kSkaleTareKmd   = 0x0F; // UtilTare

	//
	// Methods
	//
	static void    UtilInserta(int16_t Cual, int16_t Valor, std::vector<guint8>& Mensaje);
	static void    UtilTare();
	static int16_t UtilCurrentPesoHW();

};
#endif