/*
  400W Digital Load with LCD and SD Card
  2021© Brian Vincenzo Castellese

  SKETCH DI TEST & DEBUG
  COLLEGARE AL MONITOR SERIALE PER LEGGERE I DATI INVIATI DALLA SCHEDA

  -- CON QUESTO CODICE NON ALIMENTARE IL CARICO DALL'INGRESSO DI POTENZA CON UN ALIMENTATORE NON LIMITATO IN CORRENTE O UNA BATTERIA. ESPLOSIONE DEI MOSFET GARANTITA --
  -- ALIMENTARE LA SCHEDA SOLTANTO ATTRAVERSO L'ENTRATA 12V o ATTRAVERSO IL PROGRAMMATORE FTDI STESSO ( 5V ), E SUCCESSIVAMENTE COLLEGARE UN ALIMENTATORE LIMITATO IN CORRENTE SULL'ALIMENTAZIONE DI POTENZA --

  L'LCD NON E' IMPLEMENTATO, DA TESTARE SEPARATAMENTE.

  SD CARD:
  SCK = 13 (PB5)
  MISO = 12 (PB4)
  MOSI = 11 (PB3)
  CS = 8 (PB0)

  LCD:
    TX = 0 (PD0)
    RX = 1 (PD1)

  LOAD:
    Vin = A0 (PC0)
    Iin = A1 (PC1)
    Gate = 10 (PB2)

  LEDs:
    V_limit = 5 (PD5)
    I_limit = 6 (PD6)
    P_limit = 7 (PD7)
*/

#include <SPI.h>
#include <SD.h>
File myFile;

#define Vratio 70.2 //(partitore 270K + 3.9K)
#define Iratio 0.005 //(current sense amplifier)
#define Rvalue 0.000375 //valore shunt in ohm

void setup() {

  //GPIO setup
  DDRD = 0b11100010;
  PORTD = 0b11100011;
  DDRC = 0b00000000;
  PORTC = 0b00000000;
  DDRB = 0b00101101;
  PORTB = 0b00010000;

  //ADC Setup
  ADMUX = (1<<REFS0); //AVcc attivo
  ADCSRA = 0b11000100; //Attiva ADC a 1MHz e avvia la prima conversione
  while(ADCSRA & (1<<ADSC)); //Aspetta la fine della conversione

  Serial.begin(9600);
  delay(500);
  Serial.println("TEST PCB AVVIATO");
  delay(500);
  Serial.println("");
  Serial.println("Test scheda SD, modalità SPI");
  if (SD.begin(8)) Serial.println("SD inizializzata"); else Serial.println("Errore SD");
  Serial.println("Creazione e apertura file test.txt");
  myFile = SD.open("test.txt", FILE_WRITE);
  if (myFile) {
    Serial.println("File aperto. Scrittura avviata");
    String textString = "se leggi questo la scheda SD è funzionante";
    myFile.print(textString);
    myFile.close();
  } else {
    Serial.println("Errore file");
  }

  Serial.println("Verifica contenuto file:");
  myFile = SD.open("test.txt");
  if (myFile) {
    Serial.println("File test.txt aperto. Contenuto file:");

    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    myFile.close();
    Serial.println("");
    Serial.println("Lettura effettuata.");
  } else {
    Serial.println("Errore apertura file test.txt");
  }

  delay(1000);
  Serial.println("");
  Serial.println("Test GPIO");
  delay(1000);
  Serial.println("Accensione Led V_Limit");
  PORTD &= 0b00011111;
  PORTD |= (1 << 5);
  delay(1000);
  Serial.println("Accensione Led I_Limit");
  PORTD &= 0b00011111;
  PORTD |= (1 << 6);
  delay(1000);
  Serial.println("Accensione Led P_Limit");
  PORTD &= 0b00011111;
  PORTD |= (1 << 7);
  delay(1000);
  Serial.println("Collegare il multimetro in modalità continuità sull'ingresso di potenza");
  delay(10000);
  Serial.println("Accensione MOSFET, verificare la presenza di cortocircuito sull'ingresso di potenza");
  PORTB |= (1 << 2);
  delay(5000);
  Serial.println("Spegnimento MOSFET, verificare la presenza di circuito aperto sull'ingresso di potenza");
  PORTB &= ~(1 << 2);
  delay(5000);

  Serial.println("");
  Serial.println("Test ADC");
  delay(1000);
  Serial.println("Collegare un alimentatore limitato a 1A sull'ingresso di potenza");
  delay(5000);
  Serial.print("Lettura tensione a vuoto. V = ");
  uint32_t Vin = ADCread(0)*(5000.0 / 1024.0)*Vratio;
  Serial.print(Vin);
  Serial.println(" mV");
  Serial.print("Lettura corrente di cortocircuito. I = ");
  PORTB |= (1 << 2);
  delay(500);
  uint32_t Iin = ADCread(1)*(5000.0 / 1024.0)*(Iratio / Rvalue);
  Serial.print(Iin);
  Serial.println(" mA");
  PORTB &= ~(1 << 2);
  Serial.print("Potenza massima dell'alimentatore P = ");
  uint32_t Pin = (Vin * Iin) / 1000000.0;
  Serial.print(Pin);
  Serial.println(" mW");

  delay(1000);
  Serial.println("Test concluso.");

}

void loop() {

}

uint16_t ADCread(uint8_t Channel){
  ADMUX &= 0b11110000;
  if(Channel < 8) ADMUX |= Channel; else ADMUX |= 0b00001111;
  ADCSRA |= (1<<ADSC); //Avvia conversione
  while(ADCSRA & (1<<ADSC)); //Aspetta la fine della conversione
  return ADC;
}
