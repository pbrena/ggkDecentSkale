//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is the Scale Controler Logic
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include "../include/Gobbledegook.h"
#include "SkaleDatServer.h"
#include "Logger.h" // To use Logger from namespace ggk 

#define _injctvalNvect(_index, _Valor, _Vector)								\
{																			\
	_Vector[_index  ] =  static_cast<guint8> ( ( 0xFF00 & _Valor ) >> 8);	\
	_Vector[_index+1] =  static_cast<guint8>   ( 0x00FF & _Valor )      ;	\
}

using ggk::Logger;
using std::chrono::seconds;

//
// Variable Declarations
// 
std::vector<guint8> 	Scale::TmpPckt; 	// Variables Temporales
std::thread 			Scale::ContinousTask; 	// Our Continous thread

HX711::AdvancedHX711* 	Scale::chipHx711;		// Interface with external electronic chip
std::mutex				Scale::Mutex;			// Used for Thread-safety <<<---
bool					Scale::BanderaCiclo;	// bandera para terminar ciclo continuo
int16_t					Scale::PesoRaw;			// Grams * 10
int16_t					Scale::PesoConTara;
int16_t					Scale::OffsetPaTara;
int16_t					Scale::DiferenciaPeso;
uint8_t					Scale::WeightStable;
bool					Scale::LedOn;
bool					Scale::GramsOn;
bool					Scale::TimerOn;
std::vector<guint8> 	Scale::MessagePckt;

// If the thread is already running, this method will fail
// Note that it shouldn't be necessary to connect manually; 
// Returns true if the Skale thread initiates otherwise false
bool  Scale::start() {
	// If the thread is already running, return failure
	if (Scale::ContinousTask.joinable())
		{ return false; }
	// otherwise Create a thread 
	try {
		Scale::ContinousTask = std::thread(Scale::runContThread);
	}
	catch(std::system_error &ex) {
		Logger::error(SSTR << "Skale thread was unable to start (code " << ex.code() << "): " << ex.what());
		return false;
	}
	Logger::trace("Skale runCont Thread initially launched");
	return true;  // success
}

// This method will block until the SkaleCont thread joins
bool  Scale::stop() {
	{  // StartScope
		// Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
		std::lock_guard<std::mutex> lk(Scale::Mutex); 
		Logger::trace("Skale Stop locks Skale Mutex to Udate");
		Scale::BanderaCiclo = false; // bandera para terminar ciclo
		Logger::trace("Skale Stop realeses Skale Mutex");
	} // EndofScope
	Logger::trace("Skale waiting for thread termination");
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	try {
		if (Scale::ContinousTask.joinable()) {
			Scale::ContinousTask.join();
			Logger::trace("Cont Thread has stopped");
		}
		else {
			Logger::trace(" > Cont Thread is not joinable at stop time");
		}
	}
	catch(std::system_error &ex) {
		if (ex.code() == std::errc::invalid_argument) {
			Logger::warn(SSTR << "Skale ContinousTask thread was not joinable (invalid Arg) during stop(): " << ex.what());
		}
		else if (ex.code() == std::errc::no_such_process) {
			Logger::warn(SSTR << "Skale ContinousTask:  NO SUCH PROCESS" << ex.what());
		}
		else if (ex.code() == std::errc::resource_deadlock_would_occur) {
			Logger::warn(SSTR << "Deadlock avoided in call to stop() (did the event thread try to stop itself?): " << ex.what());
		}
		else {
			Logger::warn(SSTR << "Unknown system_error code (" << ex.code() << ") during stop(): " << ex.what());
		}
		return false;
	}
	return true;
	Logger::trace("Skale Stop Realeses Skale Mutex");
}

std::vector<guint8> Scale::CurrentPacket() {
	static std::vector<guint8> TmpVect;
	TmpVect.clear();
	// Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
	std::lock_guard<std::mutex> lk(Scale::Mutex); 
	// el MessagePckt se actualiza continuamente, es por eso que se copia a Mensaje Temporal
	// de forma "Thread Safe"
	TmpVect = Scale::MessagePckt; 
	return(TmpVect);
}

// Ojo: en este otro ejemplo como que se pasa apuntador tambien (pero de otra forma
// diferente a "by Reference" creo se copia el apuntador
// bool Skale::ProcesKmd(const std::vector<guint8> &SkaleKmd)
// Optaremos por: by Reference: use & example:  vector<"type">& Parm
bool  Scale::ProcesKmd( std::vector<guint8>&  SkaleKmd ) {
	// Verify Parity
	if  ( SkaleKmd[6] != Scale::calcXor(SkaleKmd) ) {    // command corrupted
		Logger::trace("Skale ProcKmd: Kmd Wrong Parity");
		return false;                 // command corrupted --> Abort
	}    
	// Otherwise... process
	if 			( SkaleKmd[1] == SKALETAREKMD   ) {
		Scale::UtilTare(); // already Thread Safe
	} else if	( SkaleKmd[1] == SKALELEDyGRKMD ) {		//LED & Grams
		// StartofScope
		// Null Action really for now
		// Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
		Logger::trace("ProcesKmd locks Skale Mutex to update LED & Gram");
		std::lock_guard<std::mutex> lk(Scale::Mutex); 
		Scale::LedOn   = true;
		Scale::GramsOn = true;
		Logger::trace("ProcesKmd releases Skale Mutex LED & Gram");
		// EndofScope  
	} else if	( SkaleKmd[1] == SKALETIMERKMD  ) {		//Timer 
		// StartofScope
		Logger::trace("ProcesKmd locks Skale Mutex to update Timer");
		std::lock_guard<std::mutex> lk(Scale::Mutex); 
		Scale::TimerOn = true;
		Logger::trace("ProcesKmd releses Skale Mutex to update Timer");
		// EndofScope  
	} else { // command corrupted --> Abort 
		Logger::trace("ProcesKmd: Kmd Wrong kmd");
		return false;
	}
	return true;
}

// This mehtod should not be called directly. Rather, it is referenced with start/stop methods
// it runs continuously on its own thread until the data server shuts down
void  Scale::runContThread() {

	Logger::trace("Entering the Skale runCont Thread");
	
	Scale::BanderaCiclo   = true;        // Ojo para mantener ciclo continuo
	Scale::PesoRaw        = 0x0000;      // Grams * 10
	Scale::PesoConTara    = 0x0000;
	Scale::OffsetPaTara   = 0x0000;
	Scale::DiferenciaPeso = 0x0000;
	Scale::LedOn          = false;
	Scale::GramsOn        = true;
	Scale::TimerOn        = false;
	// It really is a fixed size array so the shrink_to_fit, vectors used just to follow D-Bus formats
	//                0-1o 1-2o 2-Peso    4-Dif     6-xor    
	Scale::MessagePckt    = {0x03,0xCE,0x00,0x00,0x00,0x00,0xCD}; Scale::MessagePckt.shrink_to_fit();	
	Scale::TmpPckt        = {0x03,0xCE,0x00,0x00,0x00,0x00,0xCD}; Scale::TmpPckt.shrink_to_fit();	

	// Local Vars
	int16_t TmpContNvoRaw;   
	bool    TmpContBandera = true;    // Continua ciclo? Inicialmente SI

	// Read Calibradion Data from Config file.
	std::ifstream ConfigFile;
	HX711::Value  zeroParm = 0;
	HX711::Value  refParm  = 0;
	ConfigFile.open("/home/awiwi/.decentskaleconfig", std::ios::in | std::ios::binary); 
	if  (ConfigFile.fail()) {
		Logger::error("runCont Thread: NO CONFIG FILE. Run Calibration: ./src/SkaleCalibr.");
		return;
	}
	ConfigFile.read((char *) &zeroParm, sizeof(zeroParm));
	ConfigFile.read((char *) &refParm,  sizeof(refParm  ));
	ConfigFile.close();

	if  ( 0 == zeroParm) {
		Logger::error("runCont Thread: SKALE MUST NEED CALIBRATION, Run command ./src/SkaleCalibr");
		return;
	}

	// Construct an AdvancedHX711 object according with GPIO phisical wired pins and 
	// chip electronics, if communication test fails, terminates execution.
	try { Scale::chipHx711  = new HX711::AdvancedHX711(
									HWDATAPIN,
									HWCLOKPIN, 
									refParm,      // de Calibracion
									zeroParm,
									HWCHIPRATE ); // de Electronica
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
	while ( !Scale::chipHx711->isReady() ) { 
		Scale::chipHx711->powerUp(); 
		std::this_thread::sleep_for(std::chrono::milliseconds(RESCANTIMEMS));
		Logger::error("runCont Thread: HX711 not ready! Retrying...");
	}
	// ELSE: loop Continuo hasta que la bandera sea actualizada por stop()
	while (TmpContBandera) {
		// Pace the cicles  
		std::this_thread::sleep_for(std::chrono::milliseconds(RESCANTIMEMS));
		// Para no alargar el bloqueo se lee en Var temporal, 
		//                               1a sola muestra (en gr)  then -> tenths of gr
		TmpContNvoRaw = Scale::chipHx711->weight(just1SAMPLE).getValue() * 10 ;
		{ // StartofScope
			std::lock_guard<std::mutex> lk(Scale::Mutex); 
			// Segnalara el fin del ciclo al cambiar la Bandera Global, 
			// Modificado por .stop()
			TmpContBandera =  Scale::BanderaCiclo;
			// 2o. Byte
			if ( TmpContNvoRaw ==  Scale::PesoRaw ) 
				{ Scale::MessagePckt[1] = SKALESTABLE; } // Es estable...
			else 
				{ Scale::MessagePckt[1] = SKALECHNING; } // Es inestable
			Scale::PesoConTara     = TmpContNvoRaw -  Scale::OffsetPaTara;
			Scale::DiferenciaPeso  = TmpContNvoRaw -  Scale::PesoRaw; 
			// 3o. y 4o. Byte Peso
			_injctvalNvect(PESO,        Scale::PesoConTara,     Scale::MessagePckt);   
			// 5o. y 6o. Byte Peso
			_injctvalNvect(DIFERENCIA,  Scale::DiferenciaPeso,  Scale::MessagePckt);	
			// 7o. Byte - Actualizar byte de paridad
			Scale::MessagePckt[6] = Scale::calcXor(Scale::MessagePckt);
			// Recordar estado actual...
			Scale::PesoRaw 		= TmpContNvoRaw; 
		} // EndofScope
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

void Scale::UtilTare() {
	// Ojo: La creacion del objeto lock "lk", Bloquea Skale Mutex
	std::lock_guard<std::mutex> lk(Scale::Mutex); 
	Logger::trace("Skale Util Tare locks Skale Mutex to Udate");
	// Actualiza informacion de Tara
	// PesoRaw	      = ????                 Peso actual se asume actualizado
	Scale::PesoConTara	  = 0x0000;          // Se reportara Cero
	Scale::OffsetPaTara   = Scale::PesoRaw;         // PesoCrudo actual es la nueva base (offset)
	Scale::DiferenciaPeso = 0x0000;
	// Se asume estabilidad x un momento
	Scale::MessagePckt[1] = SKALESTABLE;     // 2o byte = Parm Estabilidad
	//	MessagePckt[2] = PesoConTara;     // 3o y 4o. bytes Peso 
	_injctvalNvect(PESO,       Scale:: PesoConTara,    Scale::MessagePckt);
	//	MessagePckt[4] = DiferenciaPeso;  // 5o y 6o. bytes Diferencia Peso 
	_injctvalNvect(DIFERENCIA, Scale::DiferenciaPeso,  Scale::MessagePckt);	           
	// Calcular y actualizar nueva paridad
	Scale::MessagePckt[6] =  Scale::calcXor(Scale::MessagePckt);      
	// Ojo: El termino del scope y destruccion del objeto, libera Skale Mutex
	Logger::trace("Util Tare Unlock Skale Mutex");
}

guint8 Scale::calcXor(std::vector<guint8> vector) {
	static guint8 TmpXor = vector[0];
	for (int i=1; i<=5; i++)
		{ TmpXor = TmpXor ^ vector[i]; }
	return (TmpXor);	 
}
