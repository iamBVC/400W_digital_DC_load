/*
 400W Digital Load with LCD and SD Card
 2021© Brian Vincenzo Castellese

 Funzionamento:
 Il carico può funzionare in 3 modalità, anche simultaneamente
 attraverso l'LCD vengono impostati i 3 parametri,
 ovvero tensione, corrente e potenza.
 Una volta settati il carico decrementerà la sua resistenza equivalente fino
 a raggiungere uno dei 3 parametri impostati,
 ad esempio se settata una potenza di 100W il carico assorbirà 100W,
 ma se la corrente o tensione settata non permette al carico di dissipare 100W
 si fermerà a quella corrente/tensione settata.

 Tensione: il carico si comporta come un diodo zener,
           mantenendo costante la tensione in ingresso al valore settato.
           si consiglia di limitare la potenza e corrente massima per non
           sovraccaricare il generatore da testare.

 Corrente: il carico assorbe una corrente costante pari al valore settato.

 Potenza: il carico assorbe una potenza costante pari al valore settato.

 Selezionare Arduino Pro Mini (ATmega328P 5V 16MHz) come scheda di destinazione


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
#include "Nextion.h"
#include <SD.h>
File myFile;

//questi parametri sono le soglie di allarme sopra le quali il carico si spegne automaticamente
#define Vmax 330000 //mVolt
#define Imax 10000  //mAmpere
#define Pmax 200000 //mWatt

#define Vratio 70.2 //(partitore 270K + 3.9K)
#define Iratio 0.005 //(current sense amplifier)
#define Rvalue 0.000375 //valore shunt in ohm

bool isOn = 0;
uint32_t Vset, Iset, Pset, Vin, Iin, Pin, time_mS, time_S = 0; //grandezze elettriche come unità*10^-3... esempio Pset = 1 -> 1mW

// Declare your Nextion objects - Example (page id = 0, component id = 1, component name = "b0") 
NexText tVin = NexText(0, 1, "tVin"); //Valore letto
NexText tIin = NexText(0, 2, "tIin"); //Valore letto
NexText tPin = NexText(0, 3, "tPin"); //Valore letto
NexButton bOn = NexButton(0, 4, "bOn"); //Pulsante
NexButton bOff = NexButton(0, 5, "bOff"); //Pulsante
NexSlider sVin = NexSlider(0, 6, "sVin"); //Valore impostato
NexText tsVin = NexText(0, 7, "tsVin"); //Valore impostato
NexSlider sIin = NexSlider(0, 8, "sIin"); //Valore impostato
NexText tsIin = NexText(0, 9, "tsIin"); //Valore impostato
NexSlider sPin = NexSlider(0, 10, "sPin"); //Valore impostato
NexText tsPin = NexText(0, 11, "tsPin"); //Valore impostato
NexText tState = NexText(0, 12, "tState"); //Testo riguardo lo stato

// Register a button object to the touch event list.  
NexTouch *nex_listen_list[] = {
  &bOn,
  &bOff,
  &sVin,
  &sIin,
  &sPin
};



void setup() {

  //GPIO setup
  DDRD = 0b11100010;
  PORTD = 0b11100011;
  DDRC = 0b00000000;
  PORTC = 0b00000000;
  DDRB = 0b00101101;
  PORTB = 0b00010000;

  //Timer1 Setup
  //Usato per generare un segnale PWM a 20KHz
  //Modalità CTC
  OCR1A = 799; //se aumentato aumenta la risoluzione del PWM ma diminuisce la frequenza !!
  OCR1B = 0; //imposta il pwm a 0, carico spento
  TCCR1A = 0b00010000;
  TCCR1B = (1<<WGM12)|(1<<CS10); //CTC con prescaler 1

  //Timer0 Setup
  //Usato come interrupt per lettura ADC 250Sa/s e impostare PWM uscita 
  //Modalità CTC
  OCR0A = 249; // (16MHz/(prescaler * frequenza)) - 1 = 249
  TCCR0A = (1<<WGM01); //CTC
  TCCR0B = (1<<CS02); //prescaler 256
  TIMSK0 = (1<<OCIE0A); //abilità l'interrupt

  //Timer2 Setup
  //Usato come interrupt per scrivere i log nella SD 
  //Modalità CTC
  OCR2A = 249;
  TCCR2A = (1<<WGM21); //CTC
  TCCR2B = (1<<CS22); //prescaler 1024
  TIMSK2 = (1<<OCIE2A);


  //ADC Setup
  ADMUX = (1<<REFS0); //AVcc attivo
  ADCSRA = 0b11000100; //Attiva ADC a 1MHz e avvia la prima conversione
  while(ADCSRA & (1<<ADSC)); //Aspetta la fine della conversione

  Serial.begin(9600);
  SD.begin(8);
  nexInit();
  bOn.attachPop(bOnPopCallback, &bOn);
  bOff.attachPop(bOffPopCallback, &bOff);
  sVin.attachPop(sVinPopCallback);
  sIin.attachPop(sIinPopCallback);
  sPin.attachPop(sPinPopCallback);
  
  PORTD &= 0b00011111; //Fine setup, spegni i led

}

void loop() {
  nexLoop(nex_listen_list);
  char value[10] = {0};
  
  utoa(Vin, value, 10);
  tVin.setText(value);

  utoa(Iin, value, 10);
  tIin.setText(value);

  utoa(Pin, value, 10);
  tPin.setText(value);
}

//Interrupt Timer0
ISR(TIMER0_COMPA_vect){
  Vin = ADCread(0)*(5000.0 / 1024.0)*Vratio;
  Iin = ADCread(1)*(5000.0 / 1024.0)*(Iratio / Rvalue);
  Pin = (Vin * Iin) / 1000000.0;
  
  if(Vin >= Vmax || Iin >= Imax || Pin >= Pmax){
    tState.setText("State: OFF");
    isOn = 0;
    Vset = 0;
    Iset = 0;
    Pset = 0;
    OCR1B = 0;
    if(Vin >= Vmax) PORTD |= (1 << 5);
    if(Iin >= Imax) PORTD |= (1 << 6);
    if(Pin >= Pmax) PORTD |= (1 << 7);
  }
  
  if(Vin > Vset && Iin < Iset && Pin < Pset){
    if(OCR1B < OCR1A) OCR1B++;
  }else{
    if(OCR1B > 0) OCR1B--;
  }
  
}

//Interrupt Timer2
ISR(TIMER2_COMPA_vect){
  if(time_mS >= 1000){
    time_S++;
    time_mS = 0;
    myFile = SD.open("log.txt", FILE_WRITE);
    if (myFile) {
      String logString = "";
      logString += time_S;
      logString += "\t";
      logString += Vin;
      logString += "\t";
      logString += Iin;
      logString += "\t";
      logString += Pin;
      logString += "\n";
      myFile.print(logString);
      myFile.close();
    }
  }else{
    time_mS++;
  }
}


uint16_t ADCread(uint8_t Channel){
  ADMUX &= 0b11110000;
  if(Channel < 8) ADMUX |= Channel; else ADMUX |= 0b00001111;
  ADCSRA |= (1<<ADSC); //Avvia conversione
  while(ADCSRA & (1<<ADSC)); //Aspetta la fine della conversione
  return ADC;
}


//Funzioni del Nextion
void bOnPopCallback(void *ptr) {
  tState.setText("State: ON");
  isOn = 1;
  sVin.getValue(&Vset);
  sIin.getValue(&Iset);
  sPin.getValue(&Pset);
  PORTD &= 0b00011111; //Spegne i led
  OCR1B = 0;
}

void bOffPopCallback(void *ptr) {
  tState.setText("State: OFF");
  isOn = 0;
  Vset = 0;
  Iset = 0;
  Pset = 0;
  OCR1B = 0;
}

void sVinPopCallback(void *ptr) {
  char value[10] = {0};
  uint32_t number = 0;
  sVin.getValue(&number);
  utoa(number/1000.0, value, 10);
  tsVin.setText(value);
  if(isOn) Vset = number; else Vset = 0;
}

void sIinPopCallback(void *ptr) {
  char value[10] = {0};
  uint32_t number = 0;
  sIin.getValue(&number);
  utoa(number/1000.0, value, 10);
  tsIin.setText(value);
  if(isOn) Iset = number; else Iset = 0;
}

void sPinPopCallback(void *ptr) {
  char value[10] = {0};
  uint32_t number = 0;
  sPin.getValue(&number);
  utoa(number/1000.0, value, 10);
  tsPin.setText(value);
  if(isOn) Pset = number; else Pset = 0;
}
