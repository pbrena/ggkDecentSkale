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

#include <glib.h>          // Para acceder a GVariant
#include <hx711/common.h>  // for Hx711 electronics interface
#include <iostream>
#include <future>          // for waiting funcion
#include <chrono>
#include <fstream>         // To Write Config File

class Skale      // Singleton (c++11)
{
public:

	//
	// Methods
	//

 // Returns true if the Skale thread initiates succesfully,
 // if the thread was already running, this method will fail
	static bool start();
 // This method will block until the SkaleCont thread joins
	static bool stop();

 // Used as the Data Getter/Setter by the Servers' "Linker": LaunchDecent.cpp
	static std::vector<guint8> CurrentPacket();
	static bool                ProcesKmd    (std::vector<guint8>& SkaleKmd);

 // Returns the instance to this singleton class Ojo: Solo en =.h
	static Skale &getInstance() {
		static Skale instance;
		return instance;
	}
 // Was required to be Public
 // Our thread interface, which simply launches our the thread processor on our Skale instance
 // This mehtod should not be called directly. Rather, it is started with start/stop methods
	static void runContThread();

private: // Practicamente todo 

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
	static HX711::AdvancedHX711* chipHx711; // Interface with external electronic chip
	static std::mutex	SkaleMutex;	   // Used for Thread-safety <<<---
	static bool			BanderaCiclo;  // bandera para terminar ciclo continuo
	static int16_t		PesoRaw;       // Grams * 10
	static int16_t		PesoConTara;
	static int16_t		OffsetPaTara;
	static int16_t		DiferenciaPeso;
	static uint8_t		WeightStable;
	static bool			LedOn;
	static bool			GramsOn;
	static bool			TimerOn;
	static std::vector<guint8> MessagePckt;

	//
	// Constants (tokens)
	//

	// hw - Info for the use of the externam Electronic Chip hx711
	#define HWDATAPIN		2	
	#define hWCLOKPIN		3	// pins - GPIO
	#define just1SAMPLE		1 	// hx711  parms
	#define HWCHIPRATE		HX711::Rate::HZ_80 

	#define RESCANTIMEMS	99
	#define PESO 			2
	#define DIFERENCIA 		4
	#define SKALESTABLE 	0xCE
	#define SKALECHNING 	0xCA
	#define SKALELEDyGRKMD  0x0A
	#define SKALETIMERKMD	0x0B
	#define SKALETAREKMD	0x0F

	//
	// Methods
	//
	static void UtilTare();
	static guint8 calcXor(std::vector<guint8> vector);										\

};
#endif
