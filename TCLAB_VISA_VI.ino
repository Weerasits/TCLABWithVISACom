/*
  TCLab USB Serial Interface for LabVIEW
  Arduino UNO R3 + LM35

  Accepts:
    <START,Q1,Q2,LED>
    START,Q1,Q2,LED\n   (or \r\n)

  Q1,Q2,LED = 0-100 (%)
  Response  : T1,T2  (°C)

  Pin Map (UNO R3):
    Q1  : D3 (PWM)
    Q2  : D5 (PWM)
    T1  : A0
    T2  : A2
    LED : D9 (PWM)

  ADC: UNO R3 = 10-bit (0–1023), VREF = 5.0V (AVcc)
  Sensor: LM35
    mV = ADC * (5000.0 / 1023.0)
    T  = mV / 10.0
*/

const int PIN_Q1  = 3;
const int PIN_Q2  = 5;
const int PIN_T1  = A0;
const int PIN_T2  = A2;
const int LED_PWM = 9;

const float ADC_MAX = 1023.0;
const float VREF_mV = 5000.0;

const float T_MAX = 75.0;

bool heaters_on = false;

static const uint8_t RX_BUF_SIZE = 80;
char    rxBuf[RX_BUF_SIZE];
uint8_t rxLen = 0;

bool inAnglePacket = false;
bool packetReady   = false;

float readTemperatureC_LM35(int pin);
void  applyOutputs(int q1, int q2, int led);
bool  parseAndHandle(char *line);
void  readSerialStream();

void setup() {
  Serial.begin(115200);

  pinMode(PIN_Q1,  OUTPUT);
  pinMode(PIN_Q2,  OUTPUT);
  pinMode(LED_PWM, OUTPUT);

  applyOutputs(0, 0, 0);

  delay(300);
  for (int i = 0; i < 10; i++) {
    analogRead(PIN_T1);
    analogRead(PIN_T2);
  }

  Serial.println("TCLab UNO R3 Ready");
}

void loop() {
  readSerialStream();

  if (packetReady) {
    packetReady = false;

    rxBuf[rxLen] = '\0';
    parseAndHandle(rxBuf);

    rxLen = 0;
    inAnglePacket = false;
  }

  if (heaters_on) {
    float t1 = readTemperatureC_LM35(PIN_T1);
    float t2 = readTemperatureC_LM35(PIN_T2);
    if (t1 >= T_MAX || t2 >= T_MAX) {
      applyOutputs(0, 0, 0);
      heaters_on = false;
      Serial.println("SAFETY_CUTOFF");
    }
  }
}

void readSerialStream() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '<') {
      rxLen = 0;
      inAnglePacket = true;
      continue;
    }

    if (inAnglePacket) {
      if (c == '>') {
        packetReady = true;
        return;
      }
      if (c == '\r' || c == '\n') continue;
      if (rxLen < RX_BUF_SIZE - 1) rxBuf[rxLen++] = c;
      continue;
    }

    if (c == '\r') continue;

    if (c == '\n') {
      if (rxLen > 0) {
        packetReady = true;
        return;
      }
      rxLen = 0;
      continue;
    }

    if (rxLen < RX_BUF_SIZE - 1) rxBuf[rxLen++] = c;
  }
}

bool parseAndHandle(char *line) {
  char *tok0 = strtok(line, ",");
  char *tok1 = strtok(NULL, ",");
  char *tok2 = strtok(NULL, ",");
  char *tok3 = strtok(NULL, ",");

  if (!tok0 || !tok1 || !tok2 || !tok3) return false;
  if (strcmp(tok0, "START") != 0) return false;

  int q1  = atoi(tok1);
  int q2  = atoi(tok2);
  int led = atoi(tok3);

  if (q1 < 0) q1 = 0; if (q1 > 100) q1 = 100;
  if (q2 < 0) q2 = 0; if (q2 > 100) q2 = 100;
  if (led < 0) led = 0; if (led > 100) led = 100;

  applyOutputs(q1, q2, led);
  heaters_on = (q1 > 0 || q2 > 0);

  float t1 = readTemperatureC_LM35(PIN_T1);
  float t2 = readTemperatureC_LM35(PIN_T2);

  if (t1 < 0) t1 = 0; if (t1 > 100) t1 = 100;
  if (t2 < 0) t2 = 0; if (t2 > 100) t2 = 100;

  Serial.print(t1, 2);
  Serial.print(",");
  Serial.println(t2, 2);

  return true;
}

void applyOutputs(int q1, int q2, int led) {
  int pwm1   = map(q1,  0, 100, 0, 255);
  int pwm2   = map(q2,  0, 100, 0, 255);
  int pwmLed = map(led, 0, 100, 0, 255);

  analogWrite(PIN_Q1,  pwm1);
  analogWrite(PIN_Q2,  pwm2);
  analogWrite(LED_PWM, pwmLed);
}

float readTemperatureC_LM35(int pin) {
  analogRead(pin);
  analogRead(pin);
  analogRead(pin);

  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }

  float adc = sum / 16.0;
  float mV  = adc * (VREF_mV / ADC_MAX);
  float T   = mV / 10.0;
  return T;
}