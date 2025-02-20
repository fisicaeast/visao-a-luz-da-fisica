#include <Wire.h>
#include <U8glib.h>

// --- Definições e Constantes ---
#define OLED_ADDRESS 0x3C
#define MY_FONT u8g_font_9x18
#define TEMPO_LIMITE_INATIVIDADE_CALIBRACAO 500  // 0.5 segundos em ms
#define ESCALA_LUZ_AMBIENTE 0.708f  // sem ponto-e-vírgula!

// Pinos
const int PIN_LED_AZUL     = 2;
const int PIN_LED_VERDE    = 3;
const int PIN_LED_VERMELHO = 4;
const int PIN_LED_OK       = 5;
const int PIN_BOTAO_Y      = 6;
const int PIN_SENSOR_LUZ   = A0;

// --- Variáveis Globais ---
unsigned long valorLuzAmbiente = 0;
int contadorPulsos = 0;
unsigned long ultimoPulso = 0;
bool sensorAnterior = false;

unsigned long periodoLaser = 0;
int limitePulsosVermelho, limitePulsosVerde, limitePulsosAzul;
unsigned long tempoLimiteInatividade = 0;

bool calibracaoConcluida = false;  // Indica se a calibração foi concluída
bool aguardandoNovoPulso = false;  // Para evitar contagens espúrias

// Variáveis para processamento não bloqueante dos pulsos
bool processandoPulsos = false;
unsigned long tempoInicioProcessoPulsos = 0;
int ledAcionado = -1;

// Variáveis para o piscar de LED de forma não bloqueante
bool blinkActive = false;
unsigned long blinkIntervalNB = 0;
unsigned long lastBlinkTimeNB = 0;
int blinkCountTarget = 0;
int blinkCountNB = 0;
int blinkPinNB = 0;

// --- Instância do Display OLED com U8glib ---
U8GLIB_SSD1306_128X64 u8g(OLED_ADDRESS);

// --- Máquina de Estados para Calibração ---
enum EstadoCalibracao {
  INICIO_CALIBRACAO,
  MEDINDO_TRANSICOES,
  CALCULANDO_RESULTADOS,
  AGUARDANDO_COR
};
EstadoCalibracao estadoAtualCalibracao = INICIO_CALIBRACAO;

// Variáveis para a calibração
unsigned long tLdrOld, t1Local;
bool estadoLdrOld;
unsigned long dutyCiclePositivoAcumuladoMs, dutyCicleNegativoAcumuladoMs;
unsigned long contadorAmostrasPositivas, contadorAmostrasNegativas;
unsigned long T2;
unsigned long ultimoPulsoLaser;

// Variáveis para o estado não bloqueante da fase de cálculo
// Fases: 0 = iniciar; 1 = aguardar 1500ms; 2 = blink LED; 3 = aguardar 2500ms e finalizar.
int faseCalculo = 0;
unsigned long tempoInicioFaseCalculo = 0;
float avgAltoGlobal = 0.0;
float avgBaixoGlobal = 0.0;
float DpGlobal = 0.0;
float DmGlobal = 0.0;

// --- Protótipos ---
void inicializarDisplay();
void configurarPinos();
int lerSensorLuz();
void atualizarContadorPulsos();
bool verificarTempoLimitePulsos();
void iniciarProcessamentoPulsos();
void processarCorPulsosNB();
void acenderLED(int pinoLED, const char* corTexto, int numPulsos);
void apagarLEDPulsos();
void determinarLuzAmbiente();
void botaoCalibracao();
void exibirMensagemCor(const char* corTexto, int numPulsos);
void exibirMensagem(const char* linha1, const char* linha2 = "", const char* linha3 = "", const char* linha4 = "");
void exibirMensagemSerial(const char* linha1, const char* linha2 = "", const char* linha3 = "", const char* linha4 = "");
void atualizarBlinkLED();
void aguardarPressionarBotao();

void inicializarCalibracaoLaser();
void estadoMedirTransicoes();
void estadoCalcularResultados();
void estadoAguardarCor();
void calibrarLaser();
void calcularPeriodoEDutyCycle(unsigned long dutyCiclePositivoAcumuladoMs, 
                                unsigned long dutyCicleNegativoAcumuladoMs,
                                unsigned long contadorAmostrasPositivas, 
                                unsigned long contadorAmostrasNegativas,
                                unsigned long &T2);
bool verificarFrequenciaAlta(unsigned long T2);
void exibirResultadosCalibracao(unsigned long T2, int Dp, int Dm);

// --- Implementação das Funções ---

// Inicializa o display usando U8glib
void inicializarDisplay() {
  Wire.begin();
  delay(100);  // Aguarda estabilização do I2C
  u8g.firstPage();
  do {
    u8g.setFont(MY_FONT);
    u8g.drawStr(0, 10, "Inicializando OLED...");
  } while (u8g.nextPage());
}

void configurarPinos() {
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AZUL, OUTPUT);
  pinMode(PIN_LED_OK, OUTPUT);
  pinMode(PIN_BOTAO_Y, INPUT);
  pinMode(PIN_SENSOR_LUZ, INPUT);
}

int lerSensorLuz() {
  return analogRead(PIN_SENSOR_LUZ);
}

void atualizarContadorPulsos() {
  if (calibracaoConcluida && !aguardandoNovoPulso) {
    int leitura = lerSensorLuz();
    bool sensorAtivo = (leitura < valorLuzAmbiente);
    if (!sensorAnterior && sensorAtivo) {
      contadorPulsos++;
      ultimoPulso = millis();
    }
    sensorAnterior = sensorAtivo;
  }
}

bool verificarTempoLimitePulsos() {
  return calibracaoConcluida && ((millis() - ultimoPulso) > tempoLimiteInatividade) && (contadorPulsos > 0);
}

// Inicia o processamento dos pulsos: acende o LED correspondente e exibe a mensagem
void iniciarProcessamentoPulsos() {
  if (!processandoPulsos) {
    if (contadorPulsos <= limitePulsosVermelho) {
      digitalWrite(PIN_LED_VERMELHO, HIGH);
      exibirMensagemCor("vermelho", contadorPulsos);
      ledAcionado = PIN_LED_VERMELHO;
    } else if (contadorPulsos <= limitePulsosVerde) {
      digitalWrite(PIN_LED_VERDE, HIGH);
      exibirMensagemCor("verde", contadorPulsos);
      ledAcionado = PIN_LED_VERDE;
    } else if (contadorPulsos <= limitePulsosAzul) {
      digitalWrite(PIN_LED_AZUL, HIGH);
      exibirMensagemCor("azul", contadorPulsos);
      ledAcionado = PIN_LED_AZUL;
    } else {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "Pulsos invalidos: %d", contadorPulsos);
      exibirMensagem(buffer);
      aguardandoNovoPulso = true;
      ledAcionado = -1;
    }
    processandoPulsos = true;
    tempoInicioProcessoPulsos = millis();
  }
}

// Verifica de forma não bloqueante se o tempo de inatividade foi atingido e, após 1000ms, desliga o LED
void processarCorPulsosNB() {
  if (!processandoPulsos && verificarTempoLimitePulsos()) {
    iniciarProcessamentoPulsos();
  }
  if (processandoPulsos && (millis() - tempoInicioProcessoPulsos >= 1000)) {
    apagarLEDPulsos();
    processandoPulsos = false;
    contadorPulsos = 0;
  }
}

void acenderLED(int pinoLED, const char* corTexto, int numPulsos) {
  digitalWrite(pinoLED, HIGH);
  exibirMensagemCor(corTexto, numPulsos);
}

void apagarLEDPulsos() {
  const int leds[] = {PIN_LED_VERMELHO, PIN_LED_VERDE, PIN_LED_AZUL, PIN_LED_OK};
  for (unsigned int i = 0; i < sizeof(leds) / sizeof(leds[0]); i++) {
    digitalWrite(leds[i], LOW);
  }
}

// Determina a luz ambiente (realizado em setup – pode ser mantido bloqueante)
void determinarLuzAmbiente() {
  long somaLuz = 0;
  int contador = 0;
  
  exibirMensagem("Determinando", "o valor da luz", "do ambiente...");
  unsigned long tempoInicial = millis();
  while (millis() - tempoInicial < 10000) {
    somaLuz += lerSensorLuz();
    contador++;
    delay(100);
  }
  
  unsigned long mediaLuz = somaLuz / contador;
  Serial.print(F("Luz ambiente: "));
  Serial.println(mediaLuz);
  valorLuzAmbiente = mediaLuz * ESCALA_LUZ_AMBIENTE;
  Serial.print(F("Luz ambiente escalada = "));
  Serial.println(valorLuzAmbiente);
  
  char buffer1[32], buffer2[32], buffer3[32], buffer4[32];
  snprintf(buffer1, sizeof(buffer1), "Luz ambiente:");
  snprintf(buffer2, sizeof(buffer2), "%lu", mediaLuz);
  snprintf(buffer3, sizeof(buffer3), "reescalada:");
  snprintf(buffer4, sizeof(buffer4), "%lu", valorLuzAmbiente);
  exibirMensagem(buffer1, buffer2, buffer3, buffer4);
  delay(10000);
}

void inicializarCalibracaoLaser() {
  dutyCiclePositivoAcumuladoMs = 0;
  dutyCicleNegativoAcumuladoMs = 0;
  contadorAmostrasPositivas = 0;
  contadorAmostrasNegativas = 0;
  T2 = 0;
  tLdrOld = millis();
  t1Local = millis();
  estadoLdrOld = false;
  ultimoPulsoLaser = millis();
  estadoAtualCalibracao = MEDINDO_TRANSICOES;
  exibirMensagem("Medindo", "transicoes", "do laser...");
  calibracaoConcluida = false;
  contadorPulsos = 0;
  aguardandoNovoPulso = false;
  faseCalculo = 0; // Reinicia a fase de cálculo
}

void estadoMedirTransicoes() {
  unsigned long tLdr;
  bool estadoLdr;
  long dtLocal;
  
  int leituraLuz = lerSensorLuz();
  estadoLdr = (leituraLuz > valorLuzAmbiente);
  tLdr = millis();
  
  if (estadoLdr != estadoLdrOld) {
    dtLocal = tLdr - tLdrOld;
    if (estadoLdrOld) {
      dutyCiclePositivoAcumuladoMs += dtLocal;
      contadorAmostrasPositivas++;
    } else {
      dutyCicleNegativoAcumuladoMs += dtLocal;
      contadorAmostrasNegativas++;
    }
    tLdrOld = tLdr;
    estadoLdrOld = estadoLdr;
    ultimoPulsoLaser = millis();
  }
  
  if (millis() - ultimoPulsoLaser > TEMPO_LIMITE_INATIVIDADE_CALIBRACAO) {
    estadoAtualCalibracao = CALCULANDO_RESULTADOS;
  }
}

void estadoCalcularResultados() {
  if (faseCalculo == 0) {
    // Realiza os cálculos iniciais
    calcularPeriodoEDutyCycle(dutyCiclePositivoAcumuladoMs, dutyCicleNegativoAcumuladoMs,
                              contadorAmostrasPositivas, contadorAmostrasNegativas, T2);
                            
    if (verificarFrequenciaAlta(T2)) {
      inicializarCalibracaoLaser();
      return;
    }
  
    if (contadorAmostrasPositivas > 0 && contadorAmostrasNegativas > 0) {
      avgAltoGlobal = (float)dutyCiclePositivoAcumuladoMs / contadorAmostrasPositivas;
      avgBaixoGlobal = (float)dutyCicleNegativoAcumuladoMs / contadorAmostrasNegativas;
      DpGlobal = (int)((avgAltoGlobal / (avgAltoGlobal + avgBaixoGlobal)) * 100.0f);
      DmGlobal = (int)((avgBaixoGlobal / (avgAltoGlobal + avgBaixoGlobal)) * 100.0f);
    }
  
    exibirResultadosCalibracao(T2, (int)DpGlobal, (int)DmGlobal);
    tempoInicioFaseCalculo = millis();
    faseCalculo = 1;
  }
  else if (faseCalculo == 1) {
    if (millis() - tempoInicioFaseCalculo >= 1500) {
      char bufferSerial1[128], bufferSerial2[128], bufferSerial3[128], temp[16];
      snprintf(bufferSerial1, sizeof(bufferSerial1), "T = %lu ms", T2);
      dtostrf(avgAltoGlobal, 5, 2, temp);
      snprintf(bufferSerial2, sizeof(bufferSerial2), "D+: %s ms", temp);
      dtostrf(avgBaixoGlobal, 5, 2, temp);
      snprintf(bufferSerial3, sizeof(bufferSerial3), "D-: %s ms", temp);
      exibirMensagemSerial(bufferSerial1, bufferSerial2, bufferSerial3);
      exibirMensagem(bufferSerial1, bufferSerial2, bufferSerial3);
      delay(2000);
  
      if (periodoLaser > 0) {
        limitePulsosVermelho = 2000 / periodoLaser;
        limitePulsosVerde = 2 * limitePulsosVermelho;
        limitePulsosAzul = 3 * limitePulsosVermelho;
        tempoLimiteInatividade = 10 * periodoLaser;
      }
      // Inicia o piscar do LED OK de forma não bloqueante
      blinkPinNB = PIN_LED_OK;
      blinkIntervalNB = periodoLaser;
      blinkCountTarget = 10;
      blinkCountNB = 0;
      lastBlinkTimeNB = millis();
      blinkActive = true;
  
      faseCalculo = 2;
    }
  }
  else if (faseCalculo == 2) {
    // Atualiza o blink LED
    atualizarBlinkLED();
    if (!blinkActive) {
      tempoInicioFaseCalculo = millis();
      faseCalculo = 3;
    }
  }
  else if (faseCalculo == 3) {
    if (millis() - tempoInicioFaseCalculo >= 2500) {
      estadoAtualCalibracao = AGUARDANDO_COR;
      exibirMensagem("Aguardando cor...");
      calibracaoConcluida = true;
      contadorPulsos = 0;
      faseCalculo = 0;  // Reinicia para a próxima calibração
    }
  }
}

void estadoAguardarCor() {
  if (aguardandoNovoPulso && lerSensorLuz() < valorLuzAmbiente) {
    aguardandoNovoPulso = false;
  }
}

void calibrarLaser() {
  switch (estadoAtualCalibracao) {
    case INICIO_CALIBRACAO:
      inicializarCalibracaoLaser();
      break;
    case MEDINDO_TRANSICOES:
      estadoMedirTransicoes();
      break;
    case CALCULANDO_RESULTADOS:
      estadoCalcularResultados();
      break;
    case AGUARDANDO_COR:
      estadoAguardarCor();
      break;
  }
}

void botaoCalibracao() {
  estadoAtualCalibracao = INICIO_CALIBRACAO;
  calibrarLaser();
}

void exibirMensagemCor(const char* corTexto, int numPulsos) {
  char linha1[32], linha2[32], linha3[32], linha4[32];
  snprintf(linha1, sizeof(linha1), "Cor detectada:");
  snprintf(linha2, sizeof(linha2), "%s,", corTexto);
  snprintf(linha3, sizeof(linha3), "num. pulsos:");
  snprintf(linha4, sizeof(linha4), "%d", numPulsos);
  exibirMensagem(linha1, linha2, linha3, linha4);
}

void exibirMensagem(const char* linha1, const char* linha2, const char* linha3, const char* linha4) {
  u8g.firstPage();
  do {
    u8g.setFont(MY_FONT);
    if (linha1 && strlen(linha1) > 0) u8g.drawStr(0, 10, linha1);
    if (linha2 && strlen(linha2) > 0) u8g.drawStr(0, 25, linha2);
    if (linha3 && strlen(linha3) > 0) u8g.drawStr(0, 40, linha3);
    if (linha4 && strlen(linha4) > 0) u8g.drawStr(0, 55, linha4);
  } while (u8g.nextPage());
}

void exibirMensagemSerial(const char* linha1, const char* linha2, const char* linha3, const char* linha4) {
  Serial.println(linha1);
  if (strlen(linha2) > 0) Serial.println(linha2);
  if (strlen(linha3) > 0) Serial.println(linha3);
  if (strlen(linha4) > 0) Serial.println(linha4);
}

// Atualiza o piscar do LED de forma não bloqueante
void atualizarBlinkLED() {
  if (blinkActive) {
    if (millis() - lastBlinkTimeNB >= blinkIntervalNB) {
      // Alterna o estado do LED
      if (digitalRead(blinkPinNB) == HIGH) {
        digitalWrite(blinkPinNB, LOW);
      } else {
        digitalWrite(blinkPinNB, HIGH);
      }
      lastBlinkTimeNB = millis();
      blinkCountNB++;
      if (blinkCountNB >= 2 * blinkCountTarget) {
        digitalWrite(blinkPinNB, LOW);
        blinkActive = false;
      }
    }
  }
}

void aguardarPressionarBotao() {
  while (digitalRead(PIN_BOTAO_Y) == LOW) {
    delay(50);
  }
}

void calcularPeriodoEDutyCycle(unsigned long dutyCiclePositivoAcumuladoMs, 
                                unsigned long dutyCicleNegativoAcumuladoMs,
                                unsigned long contadorAmostrasPositivas, 
                                unsigned long contadorAmostrasNegativas,
                                unsigned long &T2) {
  unsigned long totalAmostras = contadorAmostrasPositivas + contadorAmostrasNegativas;
  if (totalAmostras > 0) {
    periodoLaser = (dutyCiclePositivoAcumuladoMs + dutyCicleNegativoAcumuladoMs) / totalAmostras;
  } else {
    periodoLaser = 0;
  }
  if (contadorAmostrasPositivas > 0 && contadorAmostrasNegativas > 0) {
    T2 = (dutyCiclePositivoAcumuladoMs / contadorAmostrasPositivas) +
         (dutyCicleNegativoAcumuladoMs / contadorAmostrasNegativas);
  } else {
    T2 = 0;
  }
}

bool verificarFrequenciaAlta(unsigned long T2) {
  if (T2 == 0) {
    exibirMensagem("Frequencia muito", "alta, diminua", "e aperte amarelo");
    aguardarPressionarBotao();
    return true;
  }
  return false;
}

void exibirResultadosCalibracao(unsigned long T2, int Dp, int Dm) {
  char linha1[32], linha2[32], linha3[32];
  snprintf(linha1, sizeof(linha1), "T = %lu ms", T2);
  snprintf(linha2, sizeof(linha2), "D+ = %d%%", Dp);
  snprintf(linha3, sizeof(linha2), "D- = %d%%", Dm);
  exibirMensagem(linha1, linha2, linha3);
  exibirMensagemSerial(linha1, linha2, linha3);
  delay(2000);
}

void setup() {
  inicializarDisplay();
  configurarPinos();
  Serial.begin(57600);
  
  determinarLuzAmbiente();
  exibirMensagem("Aperte o botao", "amarelo para", "calibrar");
  aguardarPressionarBotao();
  botaoCalibracao();
}

void loop() {
  atualizarContadorPulsos();
  processarCorPulsosNB();
  calibrarLaser();
  atualizarBlinkLED();  // Atualiza o piscar do LED se ativo
  
  if (digitalRead(PIN_BOTAO_Y) == HIGH) {
    botaoCalibracao();  // Permite recalibrar
  }
}
