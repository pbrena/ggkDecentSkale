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
#include "Logger.h"

#include <thread>
#include <mutex>

#include "../include/Gobbledegook.h"
#include "SkaleDatServer.h"

// Logger & HciAdapter Count from namespace ggk 
using ggk::Logger;

std::mutex Skale::SkaleMutex; // explicit intiialization might be needed
// Variable initial values
bool    Skale::BanderaCiclo    = true;       // bandera para mantener ciclo continuo
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

// Ojo puede ser llamado hasta cada 1/10 de seg
std::vector<guint8> Skale::SkaleResponce()
{
		// Auto-start thead??
	if (!Skale::contThread.joinable() && !Skale::start())
	{
		Logger::error("Skale failed to start");
		return {}; // Null vector
	}
	// Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
	std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 
	// Read informacion
	std::vector<guint8> TmpReport = Skale::WeightReport; 
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
		return false;                 // command corrupted --> Abort
	}                
 
	 // Otherwise... process
 	guint8 leeKmd = SkaleKmd[1];
	switch( leeKmd ) {
		case kSkaleTareKmd:     //UtilTare
			Skale::getInstance().UtilTare();	 // Es Thread Safe
		break;  
		case kSkaleLEDnGrKmd :  //LED & Grams
		 // Null Action for now
		 // Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
			if (true) { // Scope only
				Logger::trace("SkaleProcKmd locks Skale Mutex to update LED & Gram");
				std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 
				Skale::LedOn   = true;
				Skale::GramsOn = true;
				Logger::trace("SkaleProcKmd releases Skale Mutex LED & Gram");
			}
		break;  
		case kSkaleTimerKmd :   //Timer 
		 // Null Action for now
		 // Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
			if (true) { // Scope only
				Logger::trace("SkaleProcKmd locks Skale Mutex to update Timer");
				std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 
				Skale::TimerOn = true;
				Logger::trace("SkaleProcKmd releses Skale Mutex to update Timer");
			}
		break;
		default : // command corrupted --> Abort 
			Logger::trace("SkaleProcKmd: Kmd Wrong kmd");
			return false;
	}
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
 // NO --- Se llama cada decima de segundo Logger::trace("UtilLeePesoHW llamado");
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

	bool BanderaLocal = true; // Continua ciclo? Inicialmente SI
	while (BanderaLocal) // Semi Continuo  OJO <---- No todas las actualizaciones se envian al Cliente
	{
	 // Pace the cicles to avoid waist CPU
		std::this_thread::sleep_for(std::chrono::milliseconds(kRescanTimeMS));
	 // Para no consumir tiempo se lee en Var temporal
		TmpNuevoPesoRaw = Skale::UtilLeePesoHW(); 

		if ( true )    // Solo para limitar scope
		{
			std::lock_guard<std::mutex> lk(Skale::SkaleMutex); 

		 	BanderaLocal = BanderaCiclo; // Actualiza desde Bandera Global, Modificado por .stop()
		 // Ojo: No asumir que si ahora el peso no cambio, antes no ha cambiado, pudo haber sido inestable

			if ( TmpNuevoPesoRaw == Skale::PesoRaw ) 
			{
			 // Ahora NO Cambio PesoRaw, antes era estable?
				if ( Skale::WeightReport[1] != kSkaleStable )
				 {   
				 // Ahora NO Cambio PesoRaw, pero antes NO era estable ==> Solo cambio estabilidad 
				 // y la Diferencia ahora es cero
					Skale::WeightReport[1] = kSkaleStable;           // 2o byte = Parm Estable

					Skale::DiferenciaPeso  = 0x0000;                 // Antes no cero
					Skale::UtilInserta(difer, Skale::DiferenciaPeso, WeightReport);	
				 // Calcular y actualizar nueva paridad
					guint8 TmpXor = Skale::WeightReport[0];        
					for(int i=1; i<=5; i++)  // Calcula xor
						{ TmpXor = TmpXor ^ Skale::WeightReport[i]; }
					Skale::WeightReport[6] = TmpXor;
				 }
				 // else {   }
				 // Ahora NO Cambio PesoRaw y adeamas YA era estable, entonces nada cambio ,
				 // TAMPOCO el reporte que  sera enviado

			}
			else
		 // Ahora, si ES inestable...
			{
			 // Se necesita actualizar TODO, peso Y diferencia
				Skale::WeightReport[1] = kSkaleChning;    // cambio? no importa, es mas rapido no preguntar
				
				Skale::PesoConTara     = TmpNuevoPesoRaw - Skale::OffsetPaTara;
				Skale::UtilInserta(peso, Skale::PesoConTara, Skale::WeightReport);   

				Skale::DiferenciaPeso  = TmpNuevoPesoRaw - Skale::PesoRaw; // se actualiza WeightReport mas abajo
				Skale::UtilInserta(difer, Skale::DiferenciaPeso, WeightReport);	
			 // Calcular y actualizar nueva paridad
				guint8 TmpXor = Skale::WeightReport[0];        
				for(int i=1; i<=5; i++)  // Calcula xor
					{ TmpXor = TmpXor ^ Skale::WeightReport[i]; }
				Skale::WeightReport[6] = TmpXor;
			 // Recordar estado actual
				Skale::PesoRaw 		   = TmpNuevoPesoRaw; 
			}
		}   // Termino del scope libera SkaleMutex
		// Notify --- Cada PERIODO haya o no cmabios
		ggkNofifyUpdatedCharacteristic("/com/decentscale/DecentScale/ReadNotify");
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
		BanderaCiclo = false; // bandera para terminar ciclo
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