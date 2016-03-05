/***************************************************************************\
   Name    : DDS_Sweeper.BAS
   Author  : Beric Dunn (K6BEZ)
   Notice  : Copyright (c) 2013  CC-BY-SA
		   : Creative Commons Attribution-ShareAlike 3.0 Unported License
   Date    : 9/26/2013
   Version : 1.0
   Notes   : Written for the Arduino Nano
		   :   Pins:
		   :   A0 - Reverse Detector Analog in
		   :   A1 - Forward Detector Analog in
		   : Modified by:
		   : Norbert Redeker (DG7EAO) 07/2014
		   : Peter Möllers, DL5YDA  01/2016
		   : TFT Display mit ILI9341 Chip, SPI, 240 x 320
		   : usglib Grafik Bibliothek   https://github.com/olikraus/ucglib/wiki
  \***************************************************************************/

#include <SPI.h>
#include "Ucglib.h"
#include <OneButton.h> //https://github.com/riyas-org/OneButton
#include <Rotary.h> //https://github.com/riyas-org/Rotary

  // Define Pins used to control AD9850 DDS
const int FQ_UD = 6;
const int SDAT = 5;
const int SCLK = 7;
const int RESET = 4;
/* Hardware:
 Rotary encoder an D2, D3, A2 (Button)
 Display Library verwendet Hardware SPI: D8, D9, D10, D11, D12, D13
 Beschaltet: 8 (RESET), 9 (D/C), 11 (MOSI), 13 (SCK),
 Nicht beschaltet: 12 (MISO), 10 (CS/SS - liegt an GND)
*/

// Variablen für Display
double vswrArray[110]; //Array für SWR
int z = 0;            // Index für Array
double SwrFreq = 14;  // Variable für Freq. mit SWR Min.
double SwrMin = 100;   // Variable für SWR Min.
double Freq1 = 1;     // Freq. Links unterste Zeile Display
double Freq2 = 15;    // Freq. Mitte unterste Zeile Display
double Freq3 = 30;    // Freq. Mitte unterste Zeile Display

// Variablen für Messung
double Fstart_MHz = 1;  // Start Frequency for sweep
double Fstop_MHz = 30;  // Stop Frequency for sweep
double current_freq_MHz; // Temp variable used during sweep
long serial_input_number; // Used to build number from serial stream
int num_steps = 100; // Number of steps to use in the sweep
char incoming_char; // Character read from serial stream

// Variablen für Menüsteuerung
const int Menu0 = 0;   // Startbildschirm
const int Menu1 = 1;   // Settings
const int SWEEP = 10;	// Messung
int Menu = 0;           // aktives Menu
int Zeile_MSettings = 0;			// aktive Zeile im Settingsmenü
bool Editmode = false;	// Element im Editiermodus

// Konstruktor für Display
Ucglib_ILI9341_18x240x320_HWSPI ucg(/*cd=*/ 9, /*cs=*/ 10, /*reset=*/ 8);

// Rotary encoder
OneButton button(A2, true); // Click button on the encoder the other end is connected to ground
Rotary r = Rotary(2, 3);	// Encoder connected to interrupt pins 2 and 3 on arduino nano (atmega328)

// the setup routine runs once when you press reset:
void setup() {
	ucg.begin(UCG_FONT_MODE_TRANSPARENT);
	ucg.setRotate90();
	ucg.setFont(ucg_font_ncenR14_tr);
	ucg.setFontPosTop();

	// Configiure DDS control pins for digital output
	pinMode(FQ_UD, OUTPUT);
	pinMode(SCLK, OUTPUT);
	pinMode(SDAT, OUTPUT);
	pinMode(RESET, OUTPUT);

	//set up the click on the encoder
	button.attachDoubleClick(doubleclick);
	button.attachClick(singleclick);

	//rotary interrupt
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
	sei();

	// Set up analog inputs on A0 and A1, internal reference voltage
	pinMode(A0, INPUT);
	pinMode(A1, INPUT);
	analogReference(INTERNAL);

	// initialize serial communication at 57600 baud
	Serial.begin(57600);

	// Reset the DDS
	digitalWrite(RESET, HIGH);
	digitalWrite(RESET, LOW);

	//Initialise the incoming serial number to zero
	serial_input_number = 0;
	welcome();
}

void welcome() {
	Menu = Menu0;
	// Schreibe Info Text auf Display
	ucg.clearScreen();
	ucg.setColor(0, 255, 0); //grün

	ucg.setPrintPos(30, 75);
	ucg.print("Arduino - Antennen - Analyzer");
	ucg.setPrintPos(55, 150);
	ucg.print("(c) DL5YDA / DG7EAO");

	ucg.setPrintPos(5, 215); ucg.setColor(255, 255, 0); // gelb
	ucg.print(">> Doppelklick fuer Einstellungen <<");
}

// Interrupt from Rotary encoder
ISR(PCINT2_vect) {
	unsigned char result = r.process();
	if ((Menu == Menu1) && (!Editmode)) {
		switch (result) {
		case DIR_CW: {
			delMarker(Zeile_MSettings);
			if (Zeile_MSettings < 8) Zeile_MSettings++;
			else Zeile_MSettings = 0;
			setMarker(Zeile_MSettings);
			break;
		}
		case DIR_CCW: {
			delMarker(Zeile_MSettings);
			if (Zeile_MSettings > 0) Zeile_MSettings--;
			else Zeile_MSettings = 8;
			setMarker(Zeile_MSettings);
			break;
		}
		default: break;
		}
		//Serial.print("Zeile: "); Serial.println(Zeile);		// debugging

	}
}

// Zeichnet Menü mit max. 9 Zeilen
// 25 px Zeilenabstand
void menu_settings()
{
	Menu = Menu1;
	//Zeile = 0;	// Startzeile
	ucg.clearScreen();
	ucg.setPrintPos(10, 15); 	ucg.print(" >>>>> 1-30 MHz <<<<<");
	ucg.setPrintPos(10, 40);	ucg.print("80m:   3500 - 4000 kHz");
	ucg.setPrintPos(10, 65);	ucg.print("40m:   7000 - 7500 kHz");
	ucg.setPrintPos(10, 90);	ucg.print("30m: 10000 - 10200 kHz");
	ucg.setPrintPos(10, 115);	ucg.print("20m: 13000 - 15000 kHz");
	ucg.setPrintPos(10, 140);	ucg.print("17m: 18000 - 18200 kHz");
	ucg.setPrintPos(10, 165);	ucg.print("15m: 21000 - 21500 kHz");
	ucg.setPrintPos(10, 190);	ucg.print("10m: 28000 - 30000 kHz");
	ucg.setPrintPos(10, 215);	ucg.print("Extras");
	setMarker(Zeile_MSettings);
}

// setzt Marker - max. 9 Zeilen
void setMarker(int Zeile) {
	ucg.setColor(0, 255, 0);	// grün
	switch (Zeile) {
	case 0: ucg.drawBox(0, 15, 3, 15); break;
	case 1: ucg.drawBox(0, 40, 3, 15); break;
	case 2: ucg.drawBox(0, 65, 3, 15); break;
	case 3: ucg.drawBox(0, 90, 3, 15); break;
	case 4: ucg.drawBox(0, 115, 3, 15); break;
	case 5: ucg.drawBox(0, 140, 3, 15); break;
	case 6: ucg.drawBox(0, 165, 3, 15); break;
	case 7: ucg.drawBox(0, 190, 3, 15); break;
	case 8: ucg.drawBox(0, 215, 3, 15); break;
	default: break;
	}
	ucg.setColor(255, 255, 255);	// weiss
}

// löscht Marker - max. 9 Zeilen 
void delMarker(int Zeile) {		
	ucg.setColor(0, 0, 0);	// schwarz
	switch (Zeile) {
	case 0: ucg.drawBox(0, 15, 3, 15); break;
	case 1: ucg.drawBox(0, 40, 3, 15); break;
	case 2: ucg.drawBox(0, 65, 3, 15); break;
	case 3: ucg.drawBox(0, 90, 3, 15); break;
	case 4: ucg.drawBox(0, 115, 3, 15); break;
	case 5: ucg.drawBox(0, 140, 3, 15); break;
	case 6: ucg.drawBox(0, 165, 3, 15); break;
	case 7: ucg.drawBox(0, 190, 3, 15); break;
	case 8: ucg.drawBox(0, 215, 3, 15); break;
	default: break;
	}
	ucg.setColor(255, 255, 255);	// weiss
}

// the loop routine runs over and over again forever:
void loop() {

	button.tick();  // keep watching the push button:
	//Check for character
	if (Serial.available() > 0) {
		incoming_char = Serial.read();
		switch (incoming_char) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			serial_input_number = serial_input_number * 10 + (incoming_char - '0');
			break;
		case 'A':
			//Turn frequency into FStart
			Fstart_MHz = ((double)serial_input_number) / 1000000;
			serial_input_number = 0;
			break;
		case 'B':
			//Turn frequency into FStop
			Fstop_MHz = ((double)serial_input_number) / 1000000;
			serial_input_number = 0;
			break;
		case 'C':
			//Turn frequency into FStart and set DDS output to single frequency
			Fstart_MHz = ((double)serial_input_number) / 1000000;
			//SetDDSFreq(Fstart_MHz);
			SetDDSFreq(Fstart_MHz * 1000000);
			delay(100);
			SetDDSFreq(Fstart_MHz * 1000000);
			serial_input_number = 0;
			break;
		case 'N':
			// Set number of steps in the sweep
			num_steps = serial_input_number;
			serial_input_number = 0;
			break;
		case 'S':
		case 's':
			perform_sweep(Fstart_MHz, Fstop_MHz);
			break;
		case '?':
			// Report current configuration to PC
			Serial.print("Start Freq:");
			Serial.println(Fstart_MHz * 1000000);
			Serial.print("Stop Freq:");
			Serial.println(Fstop_MHz * 1000000);
			Serial.print("Num Steps:");
			Serial.println(num_steps);
			break;
		}
		Serial.flush();
	}

	/*
	  //Perform Sweep nach Interrupt PIN2 oder 3
	  // ingnoriere Startup Interrupts durch counter
	  if (flag == 1 && counter >2)
	  {
	  flag = 0;
	  perform_sweep();
	  }
	*/

}

void perform_sweep(double Start_MHz, double End_MHz) {
	Fstart_MHz = Start_MHz; Fstop_MHz = End_MHz;
	double FWD = 0;
	double REV = 0;
	double VSWR;
	double Fstep_MHz = (Fstop_MHz - Fstart_MHz) / num_steps;

	Menu = SWEEP;
	z = 0;
	SwrMin = 100;

	ucg.clearScreen();
	ucg.setPrintPos(180, 15);
	ucg.print("Messvorgang...");

	// Start loop
	for (int i = 0; i <= num_steps; i++) {
		// Calculate current frequency
		current_freq_MHz = Fstart_MHz + i * Fstep_MHz;
		// Set DDS to current frequency
		SetDDSFreq(current_freq_MHz * 1000000);
		// Wait a little for settling
		//delay(10);
		delay(100);
		// Read the forward and reverse voltages
		REV = analogRead(A0);
		FWD = analogRead(A1);

		//Offset Korrektur
		REV = REV - 5;

		if (REV >= FWD) {
			REV = FWD - 1;
		}

		if (REV < 1) {
			REV = 1;
		}

		VSWR = (FWD + REV) / (FWD - REV);

		//Skalieren für Ausgabe
		VSWR = VSWR * 1000;


		// Send current line back to PC over serial bus
		Serial.print(current_freq_MHz * 1000000);
		Serial.print(",0,");
		Serial.print(VSWR);
		Serial.print(",");
		Serial.print(FWD);
		Serial.print(",");
		Serial.println(REV);


		// Übergebe SWR an Array
		// ERmittele Freq bei niedrigsten SWR
		vswrArray[z] = VSWR / 1000;

		if (vswrArray[z] > 10) vswrArray[z] = 10;

		if (vswrArray[z] < SwrMin && vswrArray[z] > 1)
		{
			SwrMin = vswrArray[z];
			SwrFreq = current_freq_MHz;
		}

		z = z + 1;

	}

	// Send "End" to PC to indicate end of sweep
	Serial.println("End");
	Serial.flush();

	//Zeichne Grid
	CreateGrid();
	//Linienfarbe
	ucg.setColor(255, 0, 0);  //rot

	// Draw Line
	// 30 = swr 10    210 = swr 0
	// Diff swr 10 = 180
	// swr 2 = 18 * 2

	double last = 10;
	double xx = 6;
	int j = 1;

	for (int i = 1; i < 103; i++) {
		xx = vswrArray[i];

		ucg.drawLine(j, 210 - last * 18, j + 1, 210 - xx * 18);
		ucg.drawLine(j + 1, 210 - last * 18, j + 2, 210 - xx * 18);

		j = j + 3;
		last = xx;
	}
}

// Setze DDS Frequenz
void SetDDSFreq(double Freq_Hz) {
	// Calculate the DDS word - from AD9850 Datasheet
	int32_t f = Freq_Hz * 4294967295 / 125000000;
	// Send one byte at a time
	for (int b = 0; b < 4; b++, f >>= 8) {
		send_byte(f & 0xFF);
	}
	// 5th byte needs to be zeros
	send_byte(0);
	// Strobe the Update pin to tell DDS to use values
	digitalWrite(FQ_UD, HIGH);
	digitalWrite(FQ_UD, LOW);
}

// Sende Daten an DDS
void send_byte(byte data_to_send) {
	// Bit bang the byte over the SPI bus
	for (int i = 0; i < 8; i++, data_to_send >>= 1) {
		// Set Data bit on output pin
		digitalWrite(SDAT, data_to_send & 0x01);
		// Strobe the clock pin
		digitalWrite(SCLK, HIGH);
		digitalWrite(SCLK, LOW);
	}
}

//Zeichne Grid auf TFT Display
void CreateGrid()
{
	Freq1 = Fstart_MHz;     // Unterste Zeile Display Freq. Links
	Freq2 = Fstart_MHz + ((Fstop_MHz - Fstart_MHz) / 2);				// Unterste Zeile Display Freq. Mitte
	Freq3 = Fstop_MHz;      // Unterste Zeile Display Freq. Rechts

	ucg.clearScreen();
	double maxSwr = 10;

	ucg.drawHLine(0, 120, 310);
	ucg.drawHLine(0, 196, 310);

	ucg.drawVLine(78, 30, 180);
	ucg.drawVLine(155, 30, 180);
	ucg.drawVLine(233, 30, 180);

	ucg.setPrintPos(0, 225);
	ucg.print(Freq1, 3);

	ucg.setPrintPos(130, 225);
	ucg.print(Freq2, 3);

	ucg.setPrintPos(260, 225);
	ucg.print(Freq3, 3);

	ucg.setPrintPos(10, 10);
	ucg.print("SWR");

	ucg.setPrintPos(70, 10);
	ucg.print(SwrMin, 2);

	ucg.setPrintPos(115, 10);
	ucg.print(">");

	ucg.setPrintPos(130, 10);
	ucg.print(maxSwr, 2);

	ucg.setPrintPos(250, 10);
	//ucg.print((freqCenter/1000000*1.05),3);
	ucg.print(SwrFreq, 3);

	ucg.drawRFrame(0, 30, 310, 180, 1);

}

//// Sweep über das gesammte Band
// Originale Tastenabfrage gelöscht
// Fstart_MHz = 1;  // Start Frequency for sweep
// Fstop_MHz = 30;  // Stop Frequency for sweep
// num_steps = 102; // Steps
// Freq1 = 1;       // Unterste Zeile Display Freq. Links
// Freq2 = 15;      // Unterste Zeile Display Freq. Mitte
// Freq3 = 30;      // Unterste Zeile Display Freq. Recht
//
// //perform_sweep();
// flag = 1;
//
//}

// Sweep mit der minimalen SWR der letzten Messung als Mittenfrequenz
void minSweep() {
	int x = SwrFreq + 0.5; //Runde auf Mhz

	if (x >= 1) Fstart_MHz = x-1;
	else Fstart_MHz = 0;			// Start Frequency for sweep
	Fstop_MHz = x+1;				// Stop Frequency for sweep
	num_steps = 102;				// Steps

	//Freq1 = Fstart_MHz;     // Unterste Zeile Display Freq. Links
	//Freq2 = x;				// Unterste Zeile Display Freq. Mitte
	//Freq3 = Fstop_MHz;      // Unterste Zeile Display Freq. Rechts

	perform_sweep(Fstart_MHz, Fstop_MHz);
}

void singleclick()
{
	switch (Menu){
	case Menu1: {
		switch (Zeile_MSettings) {
		case 0: perform_sweep(1, 30); break;
		case 1: perform_sweep(3.5, 4); break;
		case 2: perform_sweep(7, 7.5); break;
		case 3: perform_sweep(10, 10.2); break;
		case 4: perform_sweep(13, 15); break;
		case 5: perform_sweep(18, 18.2); break;
		case 6: perform_sweep(21, 21.5); break;
		case 7: perform_sweep(28, 30); break;
		case 8: break;
		default: break;}
	}; break;
	case SWEEP: minSweep(); break;
	default: break;
	};

}
void doubleclick()
{
	switch (Menu) {
	case Menu0: menu_settings(); break;
	case SWEEP: menu_settings(); break;
	default: break;
	}
}
