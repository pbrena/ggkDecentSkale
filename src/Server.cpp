// Copyright 2017-2019 Paul Nettle
//
// This file is part of Gobbledegook.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is the money file. This is your server description and complete implementation. If you want to 
// add or remove a Bluetooth service, alter its behavior, add or remove characteristics or descriptors 
// (and more), then this is your new home.
//
// >>
// >>>  DISCUSSION   SEE ServerOrigin.cpp
// >>


#include <algorithm>

#include "Server.h"
#include "ServerUtils.h"
#include "Utils.h"
#include "Globals.h"
#include "DBusObject.h"
#include "DBusInterface.h"
#include "GattProperty.h"
#include "GattService.h"
#include "GattUuid.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "Logger.h"

namespace ggk {

// There's a good chance there will be a bunch of unused parameters from the lambda macros
#if defined(__GNUC__) && defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

// Our one and only server. It's global.
std::shared_ptr<Server> TheServer = nullptr;

// -----------------------------------------------------------------------------
// Object implementation
// -----------------------------------------------------------------------------

Server::Server(const std::string &serviceName, const std::string &advertisingName, const std::string &advertisingShortName, GGKServerDataGetter getter, GGKServerDataSetter setter)
{
	// Save our names
	this->serviceName = serviceName;
	std::transform(this->serviceName.begin(), this->serviceName.end(), this->serviceName.begin(), ::tolower);
	this->advertisingName = advertisingName;
	this->advertisingShortName = advertisingShortName;

	// Register getter & setter for server data
	dataGetter = getter;
	dataSetter = setter;

	// Adapter configuration flags - set these flags based on how you want the adapter configured
	enableBREDR = false;
	enableSecureConnection = false;
	enableConnectable = true;
	enableDiscoverable = true;
	enableAdvertising = true;
	enableBondable = false;

	//
	// Define the server
	//

	// Create the root D-Bus object and push it into the list
	objects.push_back(DBusObject(DBusObjectPath() + "com" + getServiceName()));

	// We're going to build off of this object, so we need to get a reference to the instance of the object as it resides in the
	// list (and not the object that would be added to the list.)
	objects.back()

	// Decent Scale: Custom read/write text string service (0000FFF0-0000-1000-8000-00805F9B34FB)
	.gattServiceBegin("decentscale", "0000FFF0-0000-1000-8000-00805F9B34FB")

		// 1st Characteristic: UUID value (custom: 0000FFF4-0000-1000-8000-00805F9B34FB)
		// Characteristic path node name: /com/decentscale/DecentScale/ReadNotify
		// Both whole and parcial path names will be used by the Data Getter/Setters 
		// for various purposes like parameter to distiguish caller and to use correct Methods
		.gattCharacteristicBegin("ReadNotify", "0000FFF4-0000-1000-8000-00805F9B34FB", {"read", "notify"})

			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
			//  getDataValue gets a named value from the server datais a templated to allow non-pointer 
			//  data of any type to be retrieved.  getDataValue in turn calls DataGetter with identifier
			//  "ReadNotify" to react accordinly so it also can be thought as identifier of what is required
			//  to respond
				std::vector<guint8> TmpResponse = self.getDataValue< std::vector<guint8> >
												( "ReadNotify",     // Identyifies Caller
												   {}  	);          // Default value empty vector
				self.methodReturnValue(pInvocation, TmpResponse, true);
			})

			// Is flagged by The Scale Data Server each 1/10 of a sec.
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				// ...but only sends a change notification if it is determined that there are any active 
				// connections at all with getActiveConnectionCount() 
				if ( 0 < HciAdapter::getInstance().getActiveConnectionCount() ) 
				{ 
				 // get message to be send back from The Scale Data Server
					std::vector<guint8> TmpResponse = self.getDataValue< std::vector<guint8> >
													( "ReadNotify",     // Identyifies Caller
													{}  	);          // Default value empty vector			
					self.sendChangeNotificationValue(pConnection, TmpResponse); 
				} 
				return true;   //  porque ????
			})

		.gattCharacteristicEnd()

		// 2nd Characteristic: String value (custom: 0000FFF4-0000-1000-8000-00805F9B34FB)
		.gattCharacteristicBegin("WriteWoResp", "000036F5-0000-1000-8000-00805F9B34FB", { "write-without-response" })

			// Standard characteristic "WriteValue" method call
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Esto parece extraer el valor escrito y recibido en Parametros con indice 0
				// Convertirlo a Vector y enviarlo al Data Server
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataValue("WriteBack", vectorFromGVariantByteArray(pAyBuffer));

				// Note: Even though the WriteValue method returns void, it's important to return like this, so that a
				// dbus "method_return" is sent, otherwise the client gets an error (ATT error code 0x0e"unlikely").
				// Only "write-without-response" works without this
				// self.methodReturnVariant(pInvocation, NULL);
			})

		.gattCharacteristicEnd()

		// 3rd Characteristic: String value (custom: 83CDC3D4-3BA2-13FC-CC5E-106C351A9352)
		.gattCharacteristicBegin("WriteBack", "83CDC3D4-3BA2-13FC-CC5E-106C351A9352", { "write" })

			// Standard characteristic "WriteValue" method call
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Esto parece extraer el valor escrito y recibido en Parametros con indice 0
				// Convertirlo a Vector y enviarlo al Data Server
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataValue("WriteBack", vectorFromGVariantByteArray(pAyBuffer));

				// Note: Even though the WriteValue method returns void, it's important to return like this, so that a
				// dbus "method_return" is sent, otherwise the client gets an error (ATT error code 0x0e"unlikely").
				// Only "write-without-response" works without this
				self.methodReturnVariant(pInvocation, NULL);
			})

		.gattCharacteristicEnd()


	.gattServiceEnd(); // << -- NOTE THE SEMICOLON

	//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
	//                                                ____ _____ ___  _____
	//                                               / ___|_   _/ _ \|  _  |
	//                                               \___ \ | || | | | |_) |
	//                                                ___) || || |_| |  __/
	//                                               |____/ |_| \___/|_|
	//
	// You probably shouldn't mess with stuff beyond this point. It is required to meet BlueZ's requirements for a GATT Service.
	//
	// >>
	// >>  WHAT IT IS
	// >>
	//
	// From the BlueZ D-Bus GATT API description (https://git.kernel.org/pub/scm/bluetooth/bluez.git/plain/doc/gatt-api.txt):
	//
	//     "To make service registration simple, BlueZ requires that all objects that belong to a GATT service be grouped under a
	//     D-Bus Object Manager that solely manages the objects of that service. Hence, the standard DBus.ObjectManager interface
	//     must be available on the root service path."
	//
	// The code below does exactly that. Notice that we're doing much of the same work that our Server description does except that
	// instead of defining our own interfaces, we're following a pre-defined standard.
	//
	// The object types and method names used in the code below may look unfamiliar compared to what you're used to seeing in the
	// Server desecription. That's because the server description uses higher level types that define a more GATT-oriented framework
	// to build your GATT services. That higher level functionality was built using a set of lower-level D-Bus-oriented framework,
	// which is used in the code below.
	//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

	// Create the root object and push it into the list. We're going to build off of this object, so we need to get a reference
	// to the instance of the object as it resides in the list (and not the object that would be added to the list.)
	//
	// This is a non-published object (as specified by the 'false' parameter in the DBusObject constructor.) This way, we can
	// include this within our server hieararchy (i.e., within the `objects` list) but it won't be exposed by BlueZ as a Bluetooth
	// service to clietns.
	objects.push_back(DBusObject(DBusObjectPath(), false));

	// Get a reference to the new object as it resides in the list
	DBusObject &objectManager = objects.back();

	// Create an interface of the standard type 'org.freedesktop.DBus.ObjectManager'
	//
	// See: https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager
	auto omInterface = std::make_shared<DBusInterface>(objectManager, "org.freedesktop.DBus.ObjectManager");

	// Add the interface to the object manager
	objectManager.addInterface(omInterface);

	// Finally, we setup the interface. We do this by adding the `GetManagedObjects` method as specified by D-Bus for the
	// 'org.freedesktop.DBus.ObjectManager' interface.
	const char *pInArgs[] = { nullptr };
	const char *pOutArgs = "a{oa{sa{sv}}}";
	omInterface->addMethod("GetManagedObjects", pInArgs, pOutArgs, INTERFACE_METHOD_CALLBACK_LAMBDA
	{
		ServerUtils::getManagedObjects(pInvocation);
	});
}

// ---------------------------------------------------------------------------------------------------------------
// Utilitarian
// ---------------------------------------------------------------------------------------------------------------

// Find a D-Bus interface within the given D-Bus object
//
// If the interface was found, it is returned, otherwise nullptr is returned
std::shared_ptr<const DBusInterface> Server::findInterface(const DBusObjectPath &objectPath, const std::string &interfaceName) const
{
	for (const DBusObject &object : objects)
	{
		std::shared_ptr<const DBusInterface> pInterface = object.findInterface(objectPath, interfaceName);
		if (pInterface != nullptr)
		{
			return pInterface;
		}
	}

	return nullptr;
}

// Find and call a D-Bus method within the given D-Bus object on the given D-Bus interface
//
// If the method was called, this method returns true, otherwise false. There is no result from the method call itself.
bool Server::callMethod(const DBusObjectPath &objectPath, const std::string &interfaceName, const std::string &methodName, GDBusConnection *pConnection, GVariant *pParameters, GDBusMethodInvocation *pInvocation, gpointer pUserData) const
{
	for (const DBusObject &object : objects)
	{
		if (object.callMethod(objectPath, interfaceName, methodName, pConnection, pParameters, pInvocation, pUserData))
		{
			return true;
		}
	}

	return false;
}

// Find a GATT Property within the given D-Bus object on the given D-Bus interface
//
// If the property was found, it is returned, otherwise nullptr is returned
const GattProperty *Server::findProperty(const DBusObjectPath &objectPath, const std::string &interfaceName, const std::string &propertyName) const
{
	std::shared_ptr<const DBusInterface> pInterface = findInterface(objectPath, interfaceName);

	// Try each of the GattInterface types that support properties?
	if (std::shared_ptr<const GattInterface> pGattInterface = TRY_GET_CONST_INTERFACE_OF_TYPE(pInterface, GattInterface))
	{
		return pGattInterface->findProperty(propertyName);
	}
	else if (std::shared_ptr<const GattService> pGattInterface = TRY_GET_CONST_INTERFACE_OF_TYPE(pInterface, GattService))
	{
		return pGattInterface->findProperty(propertyName);
	}
	else if (std::shared_ptr<const GattCharacteristic> pGattInterface = TRY_GET_CONST_INTERFACE_OF_TYPE(pInterface, GattCharacteristic))
	{
		return pGattInterface->findProperty(propertyName);
	}

	return nullptr;
}


// Basado en Utils::stringFromGVariantByteArray
// Extracts a VECTOR from an array of bytes ("ay")
std::vector<guint8> vectorFromGVariantByteArray(const GVariant *pVariant)
{
	gsize size;
	gconstpointer pPtr = g_variant_get_fixed_array(const_cast<GVariant *>(pVariant), &size, 1);
	std::vector<guint8> array(size + 1, 0);
	memcpy(array.data(), pPtr, size);
	return array;
}

}; // namespace ggk
