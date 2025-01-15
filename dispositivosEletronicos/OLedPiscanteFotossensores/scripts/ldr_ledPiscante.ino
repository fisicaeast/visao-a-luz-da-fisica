#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display = Adafruit_SSD1306();

void calibra();
//SDA --> A4
//SCL --> A5

int volPWM=0;
int valor_pot=0, maxValorLuz;
int pinLed=12;
int pinLed2=11;
int pinoBot=6;
unsigned long t1, t2, ttemp, tLdr, tLdrOld, dt = 60;
bool estado = false, estadoLdr = false, estadoLdrOld = false;
bool teste = false;

int pinoSensorLuz = A1;          	 
int valorLuz = 0;
int sitBotao = 0;

unsigned long t1Local, Dc1p, Dc2p, Dc1m, Dc2m, T1, T2;
unsigned long contaP1, contaM1, contaP2, contaM2;

void setup() {
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setRotation(2);
  display.clearDisplay();
 
  Serial.begin(57600);
  pinMode(pinLed,OUTPUT);
  pinMode(pinLed2,OUTPUT);
  pinMode(pinoBot, INPUT);
  t1 = millis();
  calibra();
}

void calibra(){
  int conta, acumula;
 
  display.clearDisplay();
  display.setCursor(1,1);
  display.print("calibrando ...");
  display.display();
 
  t1Local = t1;
  maxValorLuz = 0;
  acumula 	= 0;
  conta   	= -3;

  do{
   t2 = millis();
   if (t2 - t1 >= 1000){
  	t1 = t2;
  	digitalWrite(pinLed,estado);
  	estado   = !estado;

  	Serial.println(maxValorLuz);	 
  	conta++;
  	if (conta > 0) acumula += maxValorLuz;
	}
	valorLuz  = analogRead(pinoSensorLuz);
	if (valorLuz > maxValorLuz) maxValorLuz = valorLuz;
  } while (millis() - t1Local < 20000);
 
  maxValorLuz = acumula / conta;
  Serial.print("valor medio = ");
  Serial.println(maxValorLuz);

  display.clearDisplay();
  display.setCursor(1,1);
  display.print("calibracao:");
  display.setCursor(1,10);
  display.print("Max. val escuro = ");
  display.print(maxValorLuz);
 
  maxValorLuz *= 0.7079;  //-3db --> 10^(-3.0/20.)
  Serial.print("-3db = ");
  Serial.println(maxValorLuz);

  display.setCursor(1,20);
  display.print("-3db = ");
  display.print(maxValorLuz);
  display.display();
}

void loop() {
  t2 = millis();
  valor_pot = analogRead(A0);
  volPWM	= map(valor_pot,0,1023,1,512);
  if (t2 - t1 >= volPWM){
	t1 = t2;
	digitalWrite(pinLed,estado);
	estado = !estado;
}
 
  valorLuz  = analogRead(pinoSensorLuz);
  if(valorLuz>maxValorLuz)
  {           	 
	digitalWrite(pinLed2,LOW);
  }
  else
  {               	 
	digitalWrite(pinLed2,HIGH);
  }
  sitBotao = digitalRead(pinoBot);
  if (sitBotao == HIGH) CalculaFeq();
}

void CalculaFeq(){
 
  display.clearDisplay();
  display.setCursor(1,1);
  display.print("Ajustando Freq ...");
  display.display();

  t1Local = millis();
  do {
	t2 = millis();
	if (t2 - t1 >= volPWM){
  	t1 = t2;
  	digitalWrite(pinLed,estado);
  	estado = !estado;
	}
    
	valorLuz  = analogRead(pinoSensorLuz);
	if(valorLuz>maxValorLuz)  {
  	digitalWrite(pinLed2,LOW);
  	estadoLdr = true;
	} else   {
  	digitalWrite(pinLed2,HIGH);
  	estadoLdr = false;
	}    
  } while (millis() - t1Local < 20000);
 
  tLdr	= millis();
  tLdrOld = tLdr;
  estadoLdrOld = estadoLdr;
 
  display.clearDisplay();
  display.setCursor(3,10);
  display.print("Calc. Freq. ...");
  display.display();
 
  Dc1p = 0;
  Dc2p = 0;
  Dc1m = 0;
  Dc2m = 0;

  contaP1 = 0;
  contaM1 = 0;
  contaP2 = 0;
  contaM2 = 0;

  t1Local = t1;
  do {
	t2 = millis();
	if (t2 - t1 >= volPWM){
  	ttemp = t1;
  	t1	= t2;
  	digitalWrite(pinLed,estado);
  	if (estado == true){
    	Dc1p += t2 - ttemp;
    	contaP1 += 1;
  	} else {
    	Dc1m += t2 - ttemp;
    	contaM1 += 1;
  	}
  	estado = !estado;
	}
    
	valorLuz  = analogRead(pinoSensorLuz);    
    
	if(valorLuz>maxValorLuz)  {
  	digitalWrite(pinLed2,LOW);
  	estadoLdr = false;
	} else   {
  	digitalWrite(pinLed2,HIGH);
  	estadoLdr = true;
	}

	if (estadoLdrOld != estadoLdr){
  	tLdrOld = tLdr;
  	tLdr = millis();
  	if (estadoLdrOld) {
    	Dc2p += tLdr - tLdrOld;
    	contaP2 += 1;
  	} else{
    	Dc2m += tLdr - tLdrOld;
    	contaM2 += 1;
  	}
  	estadoLdrOld = estadoLdr;
	}
    
  } while (millis() - t1Local < 20000);

  Dc1p = (float) Dc1p / (float) contaP1;
  Dc1m = (float) Dc1m / (float) contaM1;
  T1   = Dc1p + Dc1m;

  Dc2p = (float) Dc2p / (float) contaP2;
  Dc2m = (float) Dc2m / (float) contaM2;
  T2   = Dc2p + Dc2m;
 
  display.clearDisplay();
  display.setCursor(1,1);
  display.print("Frequencias e Duty:");
  display.setCursor(1,10);
  display.print("Led: ");
  display.print(T1);
  display.print(" ms, ");
  display.print((int) (100.0 * (float) Dc1p / (float) T1));
  display.print(", ");
  display.print((int) (100.0 * (float) Dc1m / (float) T1));

  display.setCursor(1,20);
  display.print("Ldr: ");
  display.print(T2);
  display.print(" ms, ");
  display.print((int) (100.0 * (float) Dc2p / (float) T2));
  display.print(", ");
  display.print((int) (100.0 * (float) Dc2m / (float) T2));
 
  display.display();
}
