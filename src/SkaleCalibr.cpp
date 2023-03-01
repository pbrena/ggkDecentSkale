// MIT License
//
// Copyright (c) 2020 Daniel Robertson
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "SkaleDatServer.h"  

using namespace HX711;
// To avoid using whole std namespace
using       std::cout;
using       std::cin;
using       std::flush;
using       std::cerr;
using       std::endl;

size_t       samples;
double       knownWeight;
Value        zeroValue;
SimpleHX711* tmphxChip;

int main() {
    try { tmphxChip = new SimpleHX711( 
							HWDATAPIN,   // hw <- Constantes del Alambreado y de la Electronica  
							hWCLOKPIN,  
			                	1,       // Numeros iniciales, requeridos para determinar los
    				            0,       // valores reales a traves el proceso de cßalibracion
    						HWCHIPRATE );              
    }
    catch(const GpioException& ex) {
        cerr << "Failed to connect to HX711 chip" << endl;
        return EXIT_FAILURE;
    }
    catch(const TimeoutException& ex) {
        cerr << "Failed to connect to HX711 chip" << endl;
        return EXIT_FAILURE;
    }

    cout    << "\x1B[2J\x1B[H"                                     //clear screen
            << "========================================" << endl  //splash
            << "HX711 Chip Calibration" << endl
            << "========================================"
            << endl << endl
            << "Reference object with known weight necesary."  << endl
            << "Calibration Units in Grams only" << endl
            << endl << endl;
    
    //known weight prompt
    cout    << endl
            << "1) Enter the weight of the object in gr. (Numbers only, eg. 13.7): ";
    cin >> knownWeight;
    cin.ignore();
    
    //samples prompt
    cout    << endl
            << "2) Enter the number of samples to take from the HX711 chip (eg. 15): ";
    cin >> samples;
    cin.ignore();

    //tare prompt
    cout    << endl << "3) Remove all objects from the scale and then press enter.";
    cin.ignore();
    cout    << endl << "Working..." << flush;

    zeroValue = tmphxChip->read(Options(samples));

    //weigh prompt
    cout    << endl << endl << "4) Place object on the scale and then press enter.";
    cin.ignore();
    cout    << endl << "Working..." << flush;

    // Process Values
    const double raw = tmphxChip->read(Options(samples));
    const double refUnitFloat = (raw - zeroValue) / knownWeight;
    Value refUnit = static_cast<Value>(round(refUnitFloat));

    delete tmphxChip;

    if(refUnit == 0) {
        refUnit = 1;
    }

    cout    << endl << endl
            << "Known weight (of your object): " << knownWeight << "grs." << endl 
            << "Raw value over " << samples << " samples: " << raw << endl
            << endl
            << "-> REFERENCE UNIT: " << refUnit << endl
            << "-> ZERO VALUE: " << zeroValue << endl
            << endl
            << "You can provide these values to the constructor when you create the "
            << "HX711 objects or later on. For example: " << endl
            << endl
            << "SimpleHX711 hxChip(dataPin, clockPin,"
            << refUnit << ", " << zeroValue
            << ");" << endl
            << "OR" << endl
            << "hxChip.setReferenceUnit("
            << refUnit
            << "); and hxChip.setOffset("
            << zeroValue
            << ");" << endl
            << endl;

    // Write calibration values to Configuration File (over previous content)
    std::ofstream ConfigFile;
    ConfigFile.open("/home/awiwi/.decentskaleconfig", std::ios::out | std::ios::binary); 
    if (ConfigFile.fail()) {
    	cout << "ERROR: .decentskaleconfig file in home Dir failed to open." << endl;
        return EXIT_FAILURE;
    }
    ConfigFile.write((char *) &zeroValue, sizeof(zeroValue));
    ConfigFile.write((char *) &refUnit,   sizeof(refUnit  ));
	ConfigFile.close();

    cout  << ".decentskaleconfig: SUCCESFULLY UPDATED!" << endl;

    return EXIT_SUCCESS;
}
