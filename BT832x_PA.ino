#define PA_PIN 17
#define CPS_PIN 6
#define LNA_PIN 19 
void BT832Xstop() {
	pinMode(CPS_PIN, OUTPUT);
	digitalWrite(CPS_PIN, HIGH);  //disable.  Active LOW
								  //while (!(digitalRead(CPS_PIN)==HIGH)) {} //wait until confirmed

	pinMode(LNA_PIN, OUTPUT);
	digitalWrite(LNA_PIN, LOW);  //disable.  active HIGH

	pinMode(PA_PIN, OUTPUT);
	digitalWrite(PA_PIN, LOW);  //disable.  active HIGH
								//while (!(digitalRead(LNA_PIN)==LOW)) {} //wait until confirmed
}
void BT832Xstart() {
	pinMode(PA_PIN, OUTPUT);
	digitalWrite(PA_PIN, HIGH);  //enable.  active HIGH
								 //while (!(digitalRead(PA_PIN)==HIGH)) {} //wait until confirmed

	pinMode(LNA_PIN, OUTPUT);
	digitalWrite(LNA_PIN, HIGH);  //enable.  active HIGH

	pinMode(CPS_PIN,OUTPUT);  
	digitalWrite(CPS_PIN, LOW);  //enable.  active LOW
								 //while (!(digitalRead(CPS_PIN)==LOW)) {} //wait until confirmed
}
/*#define PA_PIN 17
#define CPS_PIN 6
#define LNA_PIN 19 

  myNrf5_pinMode(CPS_PIN,OUTPUT_H0H1);
  digitalWrite(CPS_PIN,HIGH);  //disable.  Active LOW
  //while (!(digitalRead(CPS_PIN)==HIGH)) {} //wait until confirmed
   
  myNrf5_pinMode(LNA_PIN,OUTPUT_H0H1);
  digitalWrite(PA_PIN,LOW);  //disable.  active HIGH
  //while (!(digitalRead(LNA_PIN)==LOW)) {} //wait until confirmed
  
  myNrf5_pinMode(PA_PIN,OUTPUT_H0H1);  
  digitalWrite(PA_PIN,HIGH);  //enable.  active HIGH
  //while (!(digitalRead(PA_PIN)==HIGH)) {} //wait until confirmed

  //myNrf5_pinMode(CPS_PIN,OUTPUT_H0H1);  
  digitalWrite(CPS_PIN,LOW);  //enable.  active LOW*/