//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is the Scale Controler
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  DISCUSSION
// >>
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// #include <string.h>
#include <chrono>
#include <future>

// To use Logger & HciAdapter Count from namespace ggk 
#include "HciAdapter.h"
#include "Logger.h"

#include <thread>
#include <mutex>

#include "../include/Gobbledegook.h"
#include "SkaleDatServer.h"

// Logger & HciAdapter Count from namespace ggk 
using ggk::Logger;
using ggk::HciAdapter;

std::mutex Skale::SkaleMutex; // explicit intiialization might be needed
// Variable initial values
bool    Skale::TerminaCont    = false;       // bandera para terminar ciclo continuo
int16_t Skale::PesoRaw        = 0x0000;      // Grams * 10
int16_t Skale::PesoRawAntes   = 0x0000;
int16_t Skale::PesoConTara    = 0x0000;
int16_t Skale::OffsetPaTara   = 0x0000;
int16_t Skale::DiferenciaPeso = 0x0000;
bool    Skale::LedOn          = false;
bool    Skale::GramsOn        = true;
bool    Skale::TimerOn        = false;
//  03=DecentMark CE=weightstable x0000=weight x0000=Change =XorValidation
std::vector<guint8> Skale::WeightReport = {0x03,0xCE,0x00,0x00,0x00,0x00,0xCD}; 

// Our Continous thread Updates Skale (Data Server) information
std::thread Skale::contThread;

std::vector<guint8> Skale::SkaleResponce()
{
		// Auto-start thead??
	if (!Skale::contThread.joinable() && !Skale::start())
	{
		Logger::error("Skale failed to start");
		return {}; // Null vector
	}
	// Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
	Logger::trace("Skale Responce locks Skale Mutex to read Vector Report");
	std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 
	// Read informacion
	std::vector<guint8> TmpReport = Skale::WeightReport; 
	Logger::trace("Skale Responce about to relese Skale Mutex");
	return TmpReport; 
}

bool Skale::SkaleProcKmd(std::vector<guint8> SkaleKmd)
{
 // Auto-start thead??
	if (!contThread.joinable() && !Skale::start())
	{
		Logger::error("Skale ProcKmd Coudnt find cont Thread");
		return false;
	}
 // Verify Parity
	char TmpXor = SkaleKmd[0] ;          
	for(int i=1; i<=5; i++)            // Calcula xor
		{ TmpXor = TmpXor ^ SkaleKmd[i]; }
	if  ( SkaleKmd[6] != TmpXor )     // command corrupted
	{ 
		Logger::trace("Skale ProcKmd: Kmd Wrong Parity");
		return false; 
	}                                 // command corrupted --> Abort
 // Otherwise... process
 
 	guint8 leeKmd = SkaleKmd[1];
	switch( leeKmd ) {
		case kSkaleTareKmd:     //UtilTare
			Skale::getInstance().UtilTare();	 // Es Thread Safe
		break;  
		case kSkaleLEDnGrKmd :  //LED & Grams
		 // Null Action for now
		 // Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
			Logger::trace("SkaleProcKmd locks Skale Mutex to update LED & Gram");
			if (true) { // Scope only
				std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 
				Skale::LedOn   = true;
				Skale::GramsOn = true;
			}
		break;  
		case kSkaleTimerKmd :   //Timer 
		 // Null Action for now
		 // Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
			if (true) { // Scope only
				Logger::trace("SkaleProcKmd locks Skale Mutex to update Timer");
				std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 
				Skale::TimerOn = true;
			}
		break;
		default : // command corrupted --> Abort 
			Logger::trace("SkaleProcKmd: Kmd Wrong kmd");
			Logger::trace("SkaleProcKmd: about to relese Skale Mutex");
		return false;
	}
	Logger::trace("SkaleProcKmd: about to relese Skale Mutex");
	return true;
}

// pudieran ser private ?

void Skale::UtilInserta(int Cual, int16_t Valor, std::vector<guint8> Mensaje)
{
	char primer = static_cast<char> ((Valor & 0xFF00U) >> 8U);
	char segund = static_cast<char>  (Valor & 0x00FFU);

	Mensaje[Cual]   = primer;
	Mensaje[Cual++] = segund;
}

void Skale::UtilTare()
{
	// Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
	std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 
	Logger::trace("Skale Util Tare locks Skale Mutex to Udate");
	// Actualiza informacion de Tara
	// 	Skale::PesoRaw	       = ????                 Peso actual se asume actualizado
	Skale::PesoConTara	  = 0x0000;           // Se reportara Cero
	Skale::OffsetPaTara   = Skale::PesoRaw;          // PesoCrudo actual es la nueva base (offset)
	Skale::DiferenciaPeso = 0x0000;
 // Se asume estabilidad x un momento
	Skale::WeightReport[1] = kSkaleStable;    // 2o byte = Parm Estabilidad
 //	WeightReport[2] = PesoConTara;     // 3o y 4o. bytes Peso 
	Skale::UtilInserta(peso,  Skale::PesoConTara,    Skale::WeightReport);
 //	WeightReport[4] = DiferenciaPeso;  // 5o y 6o. bytes Diferencia Peso 
	Skale::UtilInserta(difer, Skale::DiferenciaPeso, Skale::WeightReport);	           
	// Calcular y actualizar nueva paridad
	guint8 TmpXor = Skale::WeightReport[0];        
	for(int i=1; i<=5; i++)  // Calcula xor
		{ TmpXor = TmpXor ^ Skale::WeightReport[i]; }
	Skale::WeightReport[6] = TmpXor;
	// Ojo: El termino del scope y destruccion del objeto, libera Skale Mutex
	Logger::trace("Util Tare Unlock Skale Mutex");
}

int16_t Skale::UtilLeePesoHW()
{
	// NO Se llama cada decima de segundo Logger::trace("UtilLeePesoHW llamado");
	//Skale::PesoRaw =  rand() % 100 - 50; 
	int16_t TmpLeePesoRaw =  0x0000;
	return  TmpLeePesoRaw;
}

// Our thread interface, which simply launches our the thread processor on our Skale instance
void runContThread()
{
	Skale::getInstance().runContThread();
}

// This mehtod should not be called directly. Rather, it is started with start/stop methods
// it runs continuously on a thread until the data server shuts down
void Skale::runContThread()
{
	Logger::trace("Entering the Skale runCont Thread");

	int16_t TmpNuevoPesoRaw;   
	// 03=Decent Type CE=weightstable 0000=weight 0000=Change =XorValidation
	//	"\x03\xCE\x00\x00\x00\x00\xCD"; 
	//  0-1o 1-2o 2-Peso 4-Dif   6-xor    

	while (!TerminaCont) // Semi Continuo  OJO <---- No todas las actualizaciones se envian al Cliente
	{
		// Pace the cicles to avoid waist CPU
		std::this_thread::sleep_for(std::chrono::milliseconds(kRescanTimeMS));
		// Para no consumir tiempo se lee en Var temporal
		TmpNuevoPesoRaw = Skale::UtilLeePesoHW(); 

		if ( true )    // Solo para limitar scope
		{
		 // Ahora si se Actualiza Informacion
			// Logger::trace("runCont Thread locks Skale Mutex to Update");
			std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 

		 // Cambio PesoRaw???
		 // Ojo: No asumir que si ahora es igual siempre a sido igual,  pudo haber sido inestable
			if ( TmpNuevoPesoRaw == Skale::PesoRaw )
			{
				if ( Skale::WeightReport[1] == kSkaleStable )
		     // Ahora es estable y ya antes lo era, entonces nada cambio -> Omitir este Ciclo
				{ 
					continue;  
				 // ******** SI CONTINUA... ALGO CAMBIO
				} 
				else 
				{   
				 // ******** SI CONTINUA... ALGO CAMBIO y no fue el pesoRaw ==> la Diferencia
				 // Ahora NuevoPesoRaw = PesoRaw anterior, pero antes era Inestable -> Actualizar
					Skale::WeightReport[1] = kSkaleStable;           // 2o byte = Parm Estable
				 // Recordar estado actual
		  		 //	Es la primera ves que coinciden... Actualizar
					Skale::DiferenciaPeso  = 0x0000;                 // Antes no cero
				 // Se actualizara mensaje mas abajo
				}
				 // No cambio el NuevoPesoRaw entonces tampoco PesoConTara 
				 // PesoConTara = TmpNuevoPesoRaw - OffsetPaTara;     
				 // PesoRaw     = TmpNuevoPesoRaw;            
								
				 // Ojo NO cambiamos  Peso reportado
				 // WeightReport[2] = PesoConTara;           // 3o y 4o. bytes Diferencia Peso 
				 // Skale::UtilInserta(peso,  PesoConTara,    WeightReport); 
			}
			else
			{
			 // Si ahora es inestable...
			 // Se necesita actualizar TODO peso a reportar Y DIFERENCIA
			 // por cierto la DIFERENCIA siempre cambia, se actualiza WeightReport mas abajo
				Skale::PesoConTara     = TmpNuevoPesoRaw - Skale::OffsetPaTara;
				Skale::UtilInserta(peso, Skale::PesoConTara, Skale::WeightReport);  // diunaves 
			 // Recordar estado actual
				Skale::DiferenciaPeso  = TmpNuevoPesoRaw - Skale::PesoRaw; // se actualiza WeightReport mas abajo
				Skale::PesoRaw 		   = TmpNuevoPesoRaw; 
				Skale::WeightReport[1] = kSkaleChning;    // antes lo era?, no importa es mas rapido no preguntar
			}
		 // Ojo En todos los casos cambio la diferencia	y el xor
			Skale::UtilInserta(difer, Skale::DiferenciaPeso, WeightReport);	
		 //	WeightReport[4] = DiferenciaPeso;  // 5o y 6o. bytes Diferencia Peso 
		 // Calcular y actualizar nueva paridad
			guint8 TmpXor = Skale::WeightReport[0];        
			for(int i=1; i<=5; i++)  // Calcula xor
				{ TmpXor = TmpXor ^ Skale::WeightReport[i]; }
			Skale::WeightReport[6] = TmpXor;
		}   // Termino del scope libera SkaleMutex
		// Logger::trace("runCont Thread unlocks SkaleMutex");

	 // The caller may choose to consult getActiveConnectionCount in order to determine 
	 // if there are any active connections before sending a change notification.
	 // active connections = Notifications ???, Probablemente No
	 	if ( 0 !=  HciAdapter::getInstance().getActiveConnectionCount() ) { 
		ggkNofifyUpdatedCharacteristic("/com/decentscale/DecentScale/ReadNotify");
	 	} 
	}
	Logger::trace("Leaving the Skale runCont Thread");
}

// If the thread is already running, this method will fail
// Note that it shouldn't be necessary to connect manually; 
// Returns true if the Skale thread initiates otherwise false
bool Skale::start()
{
	// If the thread is already running, return failure
	// No parece Jalar ????
	if (contThread.joinable())
		{ return false; }

	// otherwise Create a thread 
	try
	{
		contThread = std::thread(runContThread);
	}
	catch(std::system_error &ex)
	{
		Logger::error(SSTR << "Skale thread was unable to start (code " << ex.code() << "): " << ex.what());
		return false;
	}
	Logger::trace("Skale runCont Thread initially launched");
	return true;  // success
}

// This method will block until the SkaleCont thread joins
bool Skale::stop()
{
	if (true) {  // just to limit scope
		// Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
		std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 
		Logger::trace("Skale Stop locks Skale Mutex to Udate");
		TerminaCont = true; // bandera para terminar ciclo
		Logger::trace("Skale Stop realeses Skale Mutex");
	}
	Logger::trace("Skale waiting for thread termination");
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	try
	{
		if (contThread.joinable())
		{
			contThread.join();
			Logger::trace("Cont Thread has stopped");
		}
		else
		{
			Logger::trace(" > Cont Thread is not joinable at stop time");
		}
	}
	catch(std::system_error &ex)
	{
		if (ex.code() == std::errc::invalid_argument)
		{
			Logger::warn(SSTR << "Skale ContThread thread was not joinable (invalid Arg) during Skale::stop(): " << ex.what());
		}
		else if (ex.code() == std::errc::no_such_process)
		{
			Logger::warn(SSTR << "Skale ContThread:  NO SUCH PROCESS" << ex.what());
		}
		else if (ex.code() == std::errc::resource_deadlock_would_occur)
		{
			Logger::warn(SSTR << "Deadlock avoided in call to Skale::stop() (did the event thread try to stop itself?): " << ex.what());
		}
		else
		{
			Logger::warn(SSTR << "Unknown system_error code (" << ex.code() << ") during Skale::stop(): " << ex.what());
		}
		return false;
	}
	return true;
	Logger::trace("Skale Stop Realeses Skale Mutex");
}