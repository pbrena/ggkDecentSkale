// Copyright 2017-2019 Paul Nettle
//
// This file is part of Gobbledegook.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is an example single-file stand-alone application that runs a
// Gobbledegook server.
//
// >>
// >>>  DISCUSSION
// >>
//
// Very little is required ("MUST") by a stand-alone application to instantiate a valid Gobbledegook server. There are also some
// things that are reocommended ("SHOULD").
//
// * A stand-alone application MUST:
//
//     * Start the server via a call to `ggkStart()`.
//
//         Once started the server will run on its own thread.
//
//         Two of the parameters to `ggkStart()` are delegates responsible for providing data accessors for the server, a
//         `GGKServerDataGetter` delegate and a 'GGKServerDataSetter' delegate. The getter method simply receives a string name (for
//         example, "battery/level") and returns a void pointer to that data (for example: `(void *)&batteryLevel`). The setter does
//         the same only in reverse.
//
//         While the server is running, you will likely need to update the data being served. This is done by calling
//         `ggkNofifyUpdatedCharacteristic()` or `ggkNofifyUpdatedDescriptor()` with the full path to the characteristic or delegate
//         whose data has been updated. This will trigger your server's `onUpdatedValue()` method, which can perform whatever
//         actions are needed such as sending out a change notification (or in BlueZ parlance, a "PropertiesChanged" signal.)
//
// * A stand-alone application SHOULD:
//
//     * Shutdown the server before termination
//
//         Triggering the server to begin shutting down is done via a call to `ggkTriggerShutdown()`. This is a non-blocking method
//         that begins the asynchronous shutdown process.
//
//         Before your application terminates, it should wait for the server to be completely stopped. This is done via a call to
//         `ggkWait()`. If the server has not yet reached the `EStopped` state when `ggkWait()` is called, it will block until the
//         server has done so.
//
//         To avoid the blocking behavior of `ggkWait()`, ensure that the server has stopped before calling it. This can be done
//         by ensuring `ggkGetServerRunState() == EStopped`. Even if the server has stopped, it is recommended to call `ggkWait()`
//         to ensure the server has cleaned up all threads and other internals.
//
//         If you want to keep things simple, there is a method `ggkShutdownAndWait()` which will trigger the shutdown and then
//         block until the server has stopped.
//
//     * Implement signal handling to provide a clean shut-down
//
//         This is done by calling `ggkTriggerShutdown()` from any signal received that can terminate your application. For an
//         example of this, search for all occurrences of the string "signalHandler" in the code below.
//
//     * Register a custom logging mechanism with the server
//
//         This is done by calling each of the log registeration methods:
//
//             `ggkLogRegisterDebug()`
//             `ggkLogRegisterInfo()`
//             `ggkLogRegisterStatus()`
//             `ggkLogRegisterWarn()`
//             `ggkLogRegisterError()`
//             `ggkLogRegisterFatal()`
//             `ggkLogRegisterAlways()`
//             `ggkLogRegisterTrace()`
//
//         Each registration method manages a different log level. For a full description of these levels, see the header comment
//         in Logger.cpp.
//
//         The code below includes a simple logging mechanism that logs to stdout and filters logs based on a few command-line
//         options to specify the level of verbosity.
//
// >>
// >>>  Building with GOBBLEDEGOOK
// >>
//
// The Gobbledegook distribution includes this file as part of the Gobbledegook files with everything compiling to a single, stand-
// alone binary. It is built this way because Gobbledegook is not intended to be a generic library. You will need to make your
// custom modifications to it. Don't worry, a lot of work went into Gobbledegook to make it almost trivial to customize
// (see Server.cpp).
//
// If it is important to you or your build process that Gobbledegook exist as a library, you are welcome to do so. Just configure
// your build process to build the Gobbledegook files (minus this file) as a library and link against that instead. All that is
// required by applications linking to a Gobbledegook library is to include `include/Gobbledegook.h`.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <signal.h>
#include <iostream>
#include <thread>
#include <sstream>

#include "../include/Gobbledegook.h"
#include "SkaleDatServer.h"

//
// Constants
//

// Maximum time to wait for any single async process to timeout during initialization
static const int kMaxAsyncInitTimeoutMS = 30 * 1000;

//
// Server data Buffer (Getter/Setter Use) Antes: Server data values
//

static std::vector<guint8> DataServCharVectorBuffr = {0x03, 0xCE, 0x00, 0x00, 0x00, 0x00, 0xCD};
// static std::string  DataTextString = "\x03\x0F\x04\x03\x02\x01\x0F"; // Falla con ceros
// std::vector<uint8_t>   TmpResp ={'\x03', '\xCE', '\x00', '\x00', '\x00', '\x00', '\xCD'};

//
// Logging
//

enum LogLevel
{
	Debug,
	Verbose,
	Normal,
	ErrorsOnly
};

// Our log level - defaulted to 'Normal' but can be modified via command-line options
LogLevel logLevel = Normal;

// Our full set of logging methods (we just log to stdout)
//
// NOTE: Some methods will only log if the appropriate `logLevel` is set
void LogDebug(const char *pText) { if (logLevel <= Debug) { std::cout << "  DEBUG: " << pText << std::endl; } }
void LogInfo(const char *pText) { if (logLevel <= Verbose) { std::cout << "   INFO: " << pText << std::endl; } }
void LogStatus(const char *pText) { if (logLevel <= Normal) { std::cout << " STATUS: " << pText << std::endl; } }
void LogWarn(const char *pText) { std::cout << "WARNING: " << pText << std::endl; }
void LogError(const char *pText) { std::cout << "!!ERROR: " << pText << std::endl; }
void LogFatal(const char *pText) { std::cout << "**FATAL: " << pText << std::endl; }
void LogAlways(const char *pText) { std::cout << "..Log..: " << pText << std::endl; }
void LogTrace(const char *pText) { std::cout << "-Trace-: " << pText << std::endl; }

//
// Signal handling
//

// We setup a couple Unix signals to perform graceful shutdown in the case of SIGTERM or get an SIGING (CTRL-C)
void signalHandler(int signum)
{
	switch (signum)
	{
		case SIGINT:
			LogStatus("SIGINT recieved, shutting down");
			ggkTriggerShutdown();
			break;
		case SIGTERM:
			LogStatus("SIGTERM recieved, shutting down");
			ggkTriggerShutdown();
			break;
	}
}

//
// Data Server management
//

// Called by the ggk server when it wants to retrieve a value 
//
// This method conforms to `GGKServerDataGetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.
const void *dataGetter(const char *pName)
{
	if (nullptr == pName)
	{
		LogError("NULL name sent to server data getter");
		return nullptr;
	}

	std::string strName = pName;

	if ( strName == "ReadNotify" )
	{
		// DataTextString = TmpResp;
		// Modifica buffer de datos con mensaje de respuesta de Skale
		DataServCharVectorBuffr  = Skale::getInstance().SkaleResponce();
		// In C++11, a new member function was added to std::vector: data(). This member function returns the 
		// address of the initial element in the container, just like &vector.front(). The advantage of this
		// member function is that it is okay to call it even if the container is empty.
		return DataServCharVectorBuffr.data();
		// LogDebug((std::string("Data getter: buffer text string set to '") + DataTextString + "'").c_str());
		// return DataTextString.c_str();
	}
	else if ( strName == "WriteWoResp" || strName ==  "WriteBack ")
	{ LogError("Warning Data getter: Write caller requested Data, Null send"); }
	else  
	{ LogError((std::string("Data getter, wrong caller name: '") + strName + "'").c_str()); }

	return nullptr;
}
// Called by the server when it wants to update a named value
//
// This method conforms to `GGKServerDataSetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. 
int dataSetter(const char *pName, const void *pData)
{
	if (nullptr == pName)
	{
		LogError("NULL name sent to server data setter");
		return 0;
	}
	if (nullptr == pData)
	{
		LogError("NULL pData sent to server data setter");
		return 0;
	}

	std::string strName = pName;

	if ( strName == "ReadNotify" )
	{
		LogError("Warning: ReadNotify caller requested to write Data"); 
		return 0;
	}
	else if ( strName == "WriteWoResp" || strName ==  "WriteBack")
	{ 
		std::vector<guint8> Kompara = {0x03, 0xCE, 0x00, 0x00, 0x00, 0x00, 0xCD} ;
		// Aqui parace resolver el valor enviado (string) por el write a partir del apuntador
		DataServCharVectorBuffr = *static_cast<const std::vector<guint8> *>(pData);
		if ( Kompara ==  DataServCharVectorBuffr )
			{ 
			LogInfo("<<<<<<<Sssssssiiiii>>>>>>>>>>>>>>>");
			} 
			LogInfo("<<<<<<<no>>>>>>>>>>>>>>>" + DataServCharVectorBuffr[0]);
			LogInfo("<<<<<<<no>>>>>>>>>>>>>>>" + DataServCharVectorBuffr[1]);
			LogInfo("<<<<<<<no>>>>>>>>>>>>>>>" + DataServCharVectorBuffr[2]);
			LogInfo("<<<<<<<no>>>>>>>>>>>>>>>" + DataServCharVectorBuffr[3]);
			LogInfo("<<<<<<<no>>>>>>>>>>>>>>>" + DataServCharVectorBuffr[4]);
			LogInfo("<<<<<<<no>>>>>>>>>>>>>>>" + DataServCharVectorBuffr[5]);
			LogInfo("<<<<<<<no>>>>>>>>>>>>>>>" + DataServCharVectorBuffr[6]);
		 //  LogDebug((std::string("Server data: komand string received: '") + DataServCharVectorBuffr + "'").c_str());
		 // Ojo llama al procesador de comandos que es boolano
		if ( !Skale::getInstance().SkaleProcKmd(DataServCharVectorBuffr) )
			{ 
				LogError("Warning: SkaleProcKmd -> false, fallo"); 
				return 0; 
			} 
		//  LogDebug((std::string("Server data: komand string received: '") + DataServCharVectorBuffr + "'").c_str());
		return 1;
	}
	else  
	{ LogError((std::string("Wrong caller name sent to server data setter: '") + strName + "'").c_str());  }

	LogWarn((std::string("Unknown name for server data setter request: '") + pName + "'").c_str());
	return 0;
}

//
// Entry point
//
int main(int argc, char **ppArgv) // Arrmts count #, Actual String Arguments
{
	// A basic command-line parser
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = ppArgv[i];
		if (arg == "-q")
		{
			logLevel = ErrorsOnly;
		}
		else if (arg == "-v")
		{
			logLevel = Verbose;
		}
		else if  (arg == "-d")
		{
			logLevel = Debug;
		}
		else
		{
			LogFatal((std::string("Unknown parameter: '") + arg + "'").c_str());
			LogFatal("");
			LogFatal("Usage: standalone [-q | -v | -d]");
			return -1;
		}
	}


	// Setup our signal handlers
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	// Register our loggers
	ggkLogRegisterDebug(LogDebug);
	ggkLogRegisterInfo(LogInfo);
	ggkLogRegisterStatus(LogStatus);
	ggkLogRegisterWarn(LogWarn);
	ggkLogRegisterError(LogError);
	ggkLogRegisterFatal(LogFatal);
	ggkLogRegisterAlways(LogAlways);
	ggkLogRegisterTrace(LogTrace);

	// Start the server's ascync processing
	//
	// This starts the server on a thread and begins the initialization process
	//
	// !!!IMPORTANT!!!
	//
	//     This first parameter (the service name) must match tha name configured in the D-Bus permissions. See the Readme.md file
	//     for more information.
	//
	// The so called (Gatt) Server  (different from Data Server)
	if (!ggkStart("decentscale", "Decent Scale", "DecentScale", dataGetter, dataSetter, kMaxAsyncInitTimeoutMS))
	{
		return -1;
	}
	 // Other Stuff can be executed while Wait for the servers shutdown process
     
	 // Start the (Skale) Data server Continous process in its own Thread
	if (!Skale::getInstance().start())
	{
		return -1;
	}
	// Wait for the server to come to a complete stop (CTRL-C from the command line)
	if (!ggkWait())
	{
		return -1;
	}	
	// Stop Data Server
	if (!Skale::getInstance().stop())
	{
		return -1;
	}
	// Return the final server health status as a success (0) or error (-1)
  	return ggkGetServerHealth() == EOk ? 0 : 1;
}