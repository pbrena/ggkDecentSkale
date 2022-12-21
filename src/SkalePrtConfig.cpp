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

#include "SkaleDatServer.h"

using namespace HX711;
// To avoid using whole std namespace
using       std::cout;
using       std::cin;
using       std::flush;
using       std::cerr;
using       std::endl;

Value zeroValue;
Value refUnit;

int main()
{
 // Read & Print Calibradion Data from Config file.
    std::ifstream ConfigFile;
    ConfigFile.open("/home/awiwi/.decentskaleconfig", std::ios::in | std::ios::binary); // write over previous content
    if  (ConfigFile.fail())  {
        cout    << "ERROR: NO CONFIG FILE. Run Calibration: ./src/SkaleCalibr" << endl;
        return EXIT_FAILURE;
	}
    ConfigFile.read((char *) &zeroValue, sizeof(zeroValue));
    ConfigFile.read((char *) &refUnit,   sizeof(refUnit  ));
	ConfigFile.close();

    cout    << endl << endl
            << "-> REFERENCE UNIT: " << refUnit << endl
            << "-> ZERO VALUE: " << zeroValue << endl
            << endl
            << "You can provide these values to the constructor when you create the "
            << "HX711 objects or later on. For example: " << endl
            << endl
            << "SimpleHX711 hx("
            << ", " << refUnit << ", " << zeroValue
            << ");" << endl
            << "OR" << endl
            << "hx.setReferenceUnit("
            << refUnit
            << "); and hx.setOffset("
            << zeroValue
            << ");" << endl
            << endl;

    return EXIT_SUCCESS;
}
