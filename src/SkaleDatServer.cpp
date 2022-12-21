//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is the Scale Controler
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// >>
// >>>  DISCUSSION
// >>
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include "../include/Gobbledegook.h"
#include "SkaleDatServer.h"
#include "Logger.h" // To use Logger from namespace ggk 

using ggk::Logger;
using std::chrono::seconds;

//
// Class for Weight Cell Electronics PCB hx711
//
using std::chrono::seconds;


//
// Variable Declarations
//
HX711::AdvancedHX711* 	Skale::chipHx711;  // Interface with external electronic chip
std::mutex 				Skale::SkaleMutex;
bool    				Skale::BanderaCiclo;  
int16_t					Skale::PesoRaw;       // Grams * 10
int16_t					Skale::PesoConTara;
int16_t					Skale::OffsetPaTara;
int16_t 				Skale::DiferenciaPeso;
bool					Skale::LedOn;
bool					Skale::GramsOn;
bool					Skale::TimerOn;
std::vector<guint8>		Skale::MessagePckt; 

//
// Variables Temporales ojo no estan en la clase Skale
//
std::vector<guint8> TmpPckt; 
guint8              TmpXor; // Ojo hay otra parecida en el Scope de Cont

//
// Methods Defined as Public in =.h
//

std::thread Skale::contThread; // Our Continous thread

// If the thread is already running, this method will fail
// Note that it shouldn't be necessary to connect manually; 
// Returns true if the Skale thread initiates otherwise false
bool Skale::start()
{
	// If the thread is already running, return failure
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
		std::lock_guard<std::mutex> lk(SkaleMutex); 
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
			Logger::warn(SSTR << "Skale ContThread thread was not joinable (invalid Arg) during stop(): " << ex.what());
		}
		else if (ex.code() == std::errc::no_such_process)
		{
			Logger::warn(SSTR << "Skale ContThread:  NO SUCH PROCESS" << ex.what());
		}
		else if (ex.code() == std::errc::resource_deadlock_would_occur)
		{
			Logger::warn(SSTR << "Deadlock avoided in call to stop() (did the event thread try to stop itself?): " << ex.what());
		}
		else
		{
			Logger::warn(SSTR << "Unknown system_error code (" << ex.code() << ") during stop(): " << ex.what());
		}
		return false;
	}
	return true;
	Logger::trace("Skale Stop Realeses Skale Mutex");
}
// Ojo: puede ser llamado hasta cada 1/10 de seg. This method uses the RETURN by reference 
// notation. BEWARE not return the reference of a local variable declared in the function 
// itself because it leads to a dangling reference.
std::vector<guint8> &Skale::CurrentPacket()
{
 // NOT alocal variable 
	TmpPckt.clear();
 // Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
	std::lock_guard<std::mutex> lk(SkaleMutex); 
 // el MessagePckt se actualiza continuamente, es por eso que se copia a Mensaje Temporal
 // de forma "Thread Safe"
	TmpPckt = MessagePckt; 
 // NOT alocal variable 
	return TmpPckt; 
}

// Ojo: en este otro ejemplo como que se pasa apuntador tambien (pero de otra forma
// diferente a "by Reference" creo se copia el apuntador
// bool Skale::ProcesKmd(const std::vector<guint8> &SkaleKmd)

// Optaremos por: by Reference: use & example:  vector<"type">& Parm
bool Skale::ProcesKmd( std::vector<guint8>& SkaleKmd )
{
 // Verify Parity
	TmpXor = SkaleKmd[0] ;          
	for ( int i=1; i<=5; i++ )            // Calcula xor
		{ TmpXor = TmpXor ^ SkaleKmd[i]; }
	if  ( SkaleKmd[6] != TmpXor )     // command corrupted
	{ 
		Logger::trace("Skale ProcKmd: Kmd Wrong Parity");
		return false;                 // command corrupted --> Abort
	}    

 // Otherwise... process
	switch( SkaleKmd[1] ) {
		case kSkaleTareKmd:      //UtilTare
			Skale::UtilTare();	 // Es Thread Safe
		return true;
		case kSkaleLEDnGrKmd :  //LED & Grams
		 // Null Action for now
		 // Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
			if (true) { // Scope only
				Logger::trace("ProcesKmd locks Skale Mutex to update LED & Gram");
				std::lock_guard<std::mutex> lk(SkaleMutex); 
				LedOn   = true;
				GramsOn = true;
				Logger::trace("ProcesKmd releases Skale Mutex LED & Gram");
			}
		return true;
		case kSkaleTimerKmd :   //Timer 
		 // Null Action for now
		 // Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
			if (true) { // Scope only
				Logger::trace("ProcesKmd locks Skale Mutex to update Timer");
				std::lock_guard<std::mutex> lk(SkaleMutex); 
				TimerOn = true;
				Logger::trace("ProcesKmd releses Skale Mutex to update Timer");
			}
		return true;
		default : // command corrupted --> Abort 
			Logger::trace("ProcesKmd: Kmd Wrong kmd");
		return false;
	}
	return true;
}

// Our thread interface, which simply launches our the thread processor on our Skale instance
// Based in example of HciAdapter Singleton
void runContThread()
{
	Skale::getInstance().runContThread();
}

// This mehtod should not be called directly. Rather, it is referenced with start/stop methods
// it runs continuously on its own thread until the data server shuts down
void Skale::runContThread()
{
	Logger::trace("Entering the Skale runCont Thread");

 //
 // Class Variables initialization Only event for the Singleton
 //
	BanderaCiclo   = true;        // Ojo para mantener ciclo continuo
	PesoRaw        = 0x0000;      // Grams * 10
	PesoConTara    = 0x0000;
	OffsetPaTara   = 0x0000;
	DiferenciaPeso = 0x0000;
	LedOn          = false;
	GramsOn        = true;
	TimerOn        = false;
	// It really is a fixed size array so the shrink_to_fit, vectors used just to follow D-Bus formats
	//                0-1o 1-2o 2-Peso    4-Dif     6-xor    
	MessagePckt    = {0x03,0xCE,0x00,0x00,0x00,0x00,0xCD}; 	MessagePckt.shrink_to_fit();
	TmpPckt        = {0x03,0xCE,0x00,0x00,0x00,0x00,0xCD}; 	TmpPckt.shrink_to_fit();

 // Methods (Cont) Local Vars
	int16_t TmpContNvoRaw;   
	guint8  TmpContXor;
	bool    TmpContBandera = true;    // Continua ciclo? Inicialmente SI
 
 // Read Calibradion Data from Config file.
    std::ifstream ConfigFile;
	HX711::Value zeroParm = 0;
	HX711::Value refParm  = 0;
    ConfigFile.open("/home/awiwi/.decentskaleconfig", std::ios::in | std::ios::binary); 
    if  (ConfigFile.fail())  {
        Logger::error("runCont Thread: NO CONFIG FILE. Run Calibration: ./src/SkaleCalibr.");
        return;
	}
    ConfigFile.read((char *) &zeroParm, sizeof(zeroParm));
    ConfigFile.read((char *) &refParm,  sizeof(refParm  ));
	ConfigFile.close();

	if  ( 0 == zeroParm)  {
		Logger::error("runCont Thread: SKALE MUST NEED CALIBRATION, Run command ./src/SkaleCalibr");
        return;
	}

 // Construct an AdvancedHX711 object according with GPIO phisical wired pins and 
 // chip electronics, if communication test fails, terminates execution.
    try {   chipHx711  = new HX711::AdvancedHX711(
									hwdataPin,
									hwclockPin, 
									refParm,      // de Calibracion
									zeroParm,
									hwchipRate ); // de Electronica
    }
    catch(const HX711::GpioException& ex) {
		Logger::error("runCont Thread: FATAL! GpioException connecting to HX711");
        return;
    }
    catch(const HX711::TimeoutException& ex) {
		Logger::error("runCont Thread: FATAL! Timedout connecting to HX711");
        return;
    }
	
	// ELSE: loop Continuo hasta que el HW este listo
	while ( ! chipHx711->isReady() ) { 
		chipHx711->powerUp(); 
		std::this_thread::sleep_for(std::chrono::milliseconds(kRescanTimeMS));
		Logger::error("runCont Thread: HX711 not ready! Retrying...");
		}

	// ELSE: loop Continuo hasta que la badera sea actualizada por stop()
	while (TmpContBandera) 
	{
	 // Pace the cicles  
		std::this_thread::sleep_for(std::chrono::milliseconds(kRescanTimeMS));

	 // Para no alargar el bloqueo se lee en Var temporal, 
	 //                                 1a sola muestra (en gr)  then -> tenths of gr
		TmpContNvoRaw = chipHx711->weight(just1Sample).getValue() * 10 ;

		if ( true )    // Solo para limitar scope
		{
			std::lock_guard<std::mutex> lk(SkaleMutex); 
			
		 // Segnalara el fin del ciclo al cambiar la Bandera Global, 
		 // Modificado por .stop()
		 	TmpContBandera = BanderaCiclo;
 			
		 // 2o. Byte
			if ( TmpContNvoRaw == PesoRaw ) 
			   { MessagePckt[1] = kSkaleStable; } // Es estable...
			else 
			   { MessagePckt[1] = kSkaleChning; }  // Es inestable
			
		 // 3o. y 4o. Byte Peso
			PesoConTara     = TmpContNvoRaw - OffsetPaTara;
			Skale::UtilInserta(kPeso,  PesoConTara,    MessagePckt);   
			
		 // 5o. y 6o. Byte Peso
			DiferenciaPeso  = TmpContNvoRaw - PesoRaw; 
			Skale::UtilInserta(kDifer, DiferenciaPeso, MessagePckt);	

		 // 7o. Byte - Actualizar byte de paridad
			TmpContXor = MessagePckt[0];        
			for(int i=1; i<=5; i++)  // Calcula xor
				{ TmpContXor = TmpContXor ^ MessagePckt[i]; }
			MessagePckt[6] = TmpContXor;

		 // Recordar estado actual...
			PesoRaw 		= TmpContNvoRaw; 
			   
		}   // Termino del scope libera SkaleMutex
		
		// NotifyUpdated <-- Cada Ciclo haya o no cambios.
		// Como respuesta la caracteristica (ReadNotify) se encargara de pedir
		// a traves del dataGetter el nuevo mensaje. Realmente solo se notifica 
		// el paso del tiempo pues el peso (y por tanto el mensaje) puede variar
		// o no.
		if (0 == ggkNofifyUpdatedCharacteristic("/com/decentscale/decentscale/ReadNotify"))
			{ Logger::error("runCont Thread: Fallo ggkNofifyUpdatedCharacteristic"); }
			
	}
	Logger::trace("Leaving the Skale runCont Thread");
}

//
// Methods Defined as Private in =.h
//
// OOOOJo: There are two ways to pass a vector to a function:
// 				Pass By value
// 				Pass By Reference
// When passed by value, a copy of the vector is created. This new copy of the vector
// is then used in the function and thus, any changes made to the vector in the function 
// do not affect the original vector.
// So to modify -- by Reference: use & example:  vector<"type">& Parm
void Skale::UtilInserta(int16_t index, int16_t Valor, std::vector<guint8>& Mensaje)
{
 // Uno de los problemas fue no poner guint8 en static_cast, era Char o uint8? 
	Mensaje[index]   =  static_cast<guint8> ((0xFF00 & Valor) >> 8);
	Mensaje[index+1] =  static_cast<guint8>  (0x00FF & Valor);
 // Otro problema: Parentesis, los que siguen al static_cast<guint8>
 // & "and" using this mask to clear out (some) 8 bits

	/* Ejemplo elegante pero depende de Big/Little Endianess
	union ByteSplit
	{
		int16_t Entero;
		guint8  Partido[2];
	};
	ByteSplit Splited;
	Splited.Entero  = Valor;
	Mensaje[Cual]   = Splited.Partido[0];    // high bits
	Mensaje[Cual++] = Splited.Partido[1];    // low  bits
	*/
}

void Skale::UtilTare()
{
 // Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
	std::lock_guard<std::mutex> lk(SkaleMutex); 
	Logger::trace("Skale Util Tare locks Skale Mutex to Udate");
 // Actualiza informacion de Tara
 // PesoRaw	      = ????                 Peso actual se asume actualizado
	PesoConTara	   = 0x0000;          // Se reportara Cero
	OffsetPaTara   = PesoRaw;         // PesoCrudo actual es la nueva base (offset)
	DiferenciaPeso = 0x0000;
 // Se asume estabilidad x un momento
	MessagePckt[1] = kSkaleStable;    // 2o byte = Parm Estabilidad
 //	MessagePckt[2] = PesoConTara;     // 3o y 4o. bytes Peso 
	Skale::UtilInserta(kPeso,  PesoConTara,    MessagePckt);
 //	MessagePckt[4] = DiferenciaPeso;  // 5o y 6o. bytes Diferencia Peso 
	Skale::UtilInserta(kDifer, DiferenciaPeso, MessagePckt);	           
 // Calcular y actualizar nueva paridad
	TmpXor = MessagePckt[0];        
	for(int i=1; i<=5; i++)  // Calcula xor
		{ TmpXor = TmpXor ^ MessagePckt[i]; }
	MessagePckt[6] = TmpXor;
 // Ojo: El termino del scope y destruccion del objeto, libera Skale Mutex
	Logger::trace("Util Tare Unlock Skale Mutex");
}

// int16_t Skale::UtilCurrentPesoHW()
// {
//  // NO Logger --- Se llama cada decima de segundo  
//  // int16_t TmpLeePesoRaw =  0xabcd;
//  // Tempo: Numeros Aleatorios
//  // INT16_MAX es "Macro constants"
// 	return  rand() % INT16_MAX;
// }
