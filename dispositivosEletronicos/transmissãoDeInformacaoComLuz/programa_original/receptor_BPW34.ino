#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display = Adafruit_SSD1306();

int pinLedB  = 3;
int pinLedG  = 4;
int pinLedR  = 5;
int pinLedOK = 6;

int pinoSensorLuz = A0;        	 
int valorLuz = 0, valorLuzFundo = 0, maxValorLuz = 0, valorLuzOld;

int tempoDelay = 500, cont=0, TR, TG, TB;
unsigned long t1, t2, conta=0, tBpw, tBpwOld, dt, dtl;
bool estado = false, estadoBpw = false, estadoBpwOld = false;

unsigned long t1Local, Dc1p, Dc2p, Dc1m, Dc2m, T1, T2;
unsigned long contaP1, contaM1, contaP2, contaM2;

unsigned long tl1, tl2;

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  //display.setRotation(2);
  display.clearDisplay();
 
  Serial.begin(57600);
  pinMode(pinLedR,OUTPUT);
  pinMode(pinLedG,OUTPUT);
  pinMode(pinLedB,OUTPUT);
  pinMode(pinLedOK,OUTPUT);
  pinMode(12,OUTPUT);
  luzDeFondo();
  calibraLaser();
  TR = 2000/dt;
  TG = 2*TR;
  TB = 3*TR;
  t2 = millis();
  valorLuzOld = analogRead(pinoSensorLuz);

  digitalWrite(pinLedR,LOW);
  digitalWrite(pinLedG,LOW);
  digitalWrite(pinLedB,LOW);

  for (int i=0; i<10; i++){
	digitalWrite(pinLedOK,HIGH);
	delay(dt);
	digitalWrite(pinLedOK,LOW);
	delay(dt);
  }
}

void luzDeFondo(){
  //Menos luz ==> valorLuzFundo ~ 1023
  //Mais luz ==> valorLuzFundo ~ 0
  display.clearDisplay();
  display.setCursor(1,1);
  display.print("Determinando valor da");
  display.setCursor(1,10);
  display.print("luz de ambiente...");
  display.display();
  t1Local = millis();
  do{
	valorLuzFundo  += analogRead(pinoSensorLuz);
	conta += 1;
	delay(100);
  } while (millis() - t1Local < 10000);
  valorLuzFundo /= conta;
 
  display.clearDisplay();
  display.setCursor(1,1);
  display.print("Determinando valor da");
  display.setCursor(1,10);
  display.print("luz de ambiente: ");
  //display.setCursor(1,20);
  //display.print("valor: ");
  display.print(valorLuzFundo);
  display.setCursor(1,20);
  display.print("Inicie o emissor ...");
  display.display();
  digitalWrite(pinLedOK,HIGH);
  delay(10000);
  tBpwOld = millis();
}

void calibraLaser(){
  int Dp, Dm;
  long dtLocal;
    
  display.clearDisplay();
  display.setCursor(1,1);
  display.print("Determinando valor do");
  display.setCursor(1,10);
  display.print("periodo do laser ..");
  display.display();
 
  t1Local = millis();
  do {
	valorLuz  = analogRead(pinoSensorLuz) - valorLuzFundo;
	t2 = millis();
	estadoBpw = valorLuz>20;
    
	tBpw = millis();
	if (estadoBpwOld != estadoBpw){
  	if (estadoBpwOld) {
    	dtLocal = tBpw - tBpwOld;
    	Dc2p += dtLocal;
    	tBpwOld = tBpw;
    	contaP2 += 1;
    	Serial.println(dtLocal/1000.0);
  	} else{
    	dtLocal = tBpw - tBpwOld;
    	Dc2m += dtLocal;
    	tBpwOld = tBpw;
    	contaM2 += 1;
    	Serial.println(dtLocal/1000.0);
  	}
  	estadoBpwOld = estadoBpw;
	}
  } while (millis() - t1Local < 10000);

 
  dt   = (Dc2p + Dc2m) / ((contaP2 + contaM2));
  Dc2p = (float) Dc2p / (contaP2);
  Dc2m = (float) Dc2m / (contaM2);
  T2   = Dc2p + Dc2m;
    
  display.clearDisplay();
  display.setCursor(1,1);
  display.print("Periodo e Duty + e -");
  display.setCursor(1,10);
  display.print("T = ");
  display.print(T2);
  display.print(" ms");

  Dp = (int) (100.0 * (float) Dc2p / (float) T2);
  Dm = (int) (100.0 * (float) Dc2m / (float) T2);

  display.setCursor(1,20);
  display.print("D+: ");
  display.print(Dp);
  display.print("%, D-: ");
  display.print(Dm);
  display.print("%");
  display.display();
  delay(10000);
 
  display.clearDisplay();
  display.setCursor(1,10);
  display.print("Escolha uma cor ..");
  display.display();
}


void loop() {
  if (analogRead(pinoSensorLuz) - valorLuzFundo > 20){
	cont += 1;
	t1 = millis();    
	delay(2*dt);
  }
 
  dtl = millis() - t1;
  if (dtl > 10*dt){
	if (cont > 0 && cont <= TR) {
  	digitalWrite(pinLedR,HIGH);
  	lcdCor(1, cont);
	} else if (cont > TR && cont <= TG) {
  	digitalWrite(pinLedG,HIGH);
  	lcdCor(2, cont);
	}else if (cont > TG && cont <= TB) {
  	digitalWrite(pinLedB,HIGH);
  	lcdCor(3, cont);
	}
	delay(1000);
    
	Serial.print(t1);
	Serial.print(", ");
	Serial.print(millis());
	Serial.print(", ");
	Serial.print(dtl);
	Serial.print(", ");
	Serial.print(t1 - millis());
	Serial.print(", ");
	Serial.println(cont);
    
    
	digitalWrite(pinLedR,LOW);
	digitalWrite(pinLedG,LOW);
	digitalWrite(pinLedB,LOW);
	cont = 0;
	t1   = millis();
  }
}

void lcdCor(int cor, int ntrem){
  display.clearDisplay();
  display.setCursor(1,1);
  display.print("Voce clicou no botao");
  display.setCursor(1,10);
  if (cor == 1){
	display.print("verrmelho");
  } else if (cor ==  2){
	display.print("verde");
  } else if (cor ==  3){
	display.print("azul");
  }
  display.print(", trem com");
  display.setCursor(1,20);
  display.print(ntrem);
  display.print(" oscilacoes");
  display.display();
}
