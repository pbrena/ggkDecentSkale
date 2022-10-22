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
// This is a single-file stand-alone application that starts the Gobbledegook server, 
// starts Skale Data Server and link them together thru DataSetter/Getter
//
// >>
// >>>  DISCUSSION See StanaloneOrig.cpp
// >>
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
static std::vector<guint8> KomndCharVectorBuffr    = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. 
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
		// Modifica buffer de datos con mensaje de respuesta de Skale
		DataServCharVectorBuffr  = Skale::getInstance().CurrentPacket();
		LogDebug("Data getter: current &DataServCharVectorBuffr replied"); 
		return &DataServCharVectorBuffr;
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
		// Aqui parace resolver el valor enviado (vector) por el write a partir del apuntador
		KomndCharVectorBuffr = *static_cast<const std::vector<guint8> *>(pData);
		 // Ojo llama al procesador de comandos que es boolano
		if ( !Skale::getInstance().SkaleProcKmd(KomndCharVectorBuffr) )
			{ 
				LogError("Warning: SkaleProcKmd -> false, fallo"); 
				return 0; 
			} 
		LogDebug((std::string("Server data: komand vector received")).c_str());
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
	if (!ggkStart("decentscale", "Decent Scale", "Decent Scale", dataGetter, dataSetter, kMaxAsyncInitTimeoutMS))
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