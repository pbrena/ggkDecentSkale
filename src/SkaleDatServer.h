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

class Skale      // Singleton (c++11)
{
public:

	//
	// Methods
	//

 // Returns the instance to this singleton class
	static Skale &getInstance()
	{
		static Skale instance;
		return instance;
	}

 // Was required to be Public
 // Our thread interface, which simply launches our the thread processor on our Skale instance
 // This mehtod should not be called directly. Rather, it is started with start/stop methods
	static void runContThread();

 // Returns true if the Skale thread initiates succesfully,
 // if the thread was already running, this method will fail
	static bool start();

 // This method will block until the SkaleCont thread joins
	static bool stop();
 // Used as the Data Getter/Setter by the Servers' "Linker": Standalone.cpp
	static std::vector<guint8> &CurrentPacket ();
	static bool                 ProcesKmd     (std::vector<guint8>& SkaleKmd);

private:

	// Private constructor for our Singleton force use of getInstanc
	Skale() { };
	//
	// Disallow copies of our Singleton (c++11)
	//
	Skale(Skale const&) = delete;
	void operator=(Skale const&) = delete;

	static std::thread contThread;

	//
	// Variables
	//

	static std::mutex	SkaleMutex;	   // Used or sake of Thread-safety <<<---
	static bool			BanderaCiclo;  // bandera para terminar ciclo continuo
	static int16_t		PesoRaw;       // Grams * 10
	static int16_t		PesoConTara;
	static int16_t		OffsetPaTara;
	static int16_t		DiferenciaPeso;
	static uint8_t		WeightStable;
	static bool			LedOn;
	static bool			GramsOn;
	static bool			TimerOn;
 // It really is a fixed size array, used just to follow D-Bus formats
 // Index: 0-1er=Decent id  1-2o Stability  2-Peso      4-Dif       6-xor      
 //        03=Decent Type   CE=weightstable xxxx=weight xxxx=Change XorValidation
	static std::vector<guint8> MessagePckt; 

	//
	// Constants
	//

	static const int16_t kRescanTimeMS   = 99;
	static const int16_t kPeso           = 2;   // Posicion del Peso
	static const int16_t kDifer          = 4;   // Posicion de la diferencia
	static const guint8  kSkaleStable    = 0xCE; // weight stable
	static const guint8  kSkaleChning    = 0xCA; // weight changing
	static const guint8  kSkaleLEDnGrKmd = 0x0A; // LED and grams on/off
	static const guint8  kSkaleTimerKmd  = 0x0B; // Timer on/off
	static const guint8  kSkaleTareKmd   = 0x0F; // UtilTare

	//
	// Methods
	//

	static void    UtilInserta(int16_t index, int16_t Valor, std::vector<guint8>& Mensaje);
	static void    UtilTare();
	static int16_t UtilCurrentPesoHW();

};
#endif