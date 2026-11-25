#include <OneWire.h>
#include <DallasTemperature.h>

// ---- Pins ----
#define ENA        PA8    // motor PWM
#define IN1        PB12
#define IN2        PB13
#define ACS_PIN    PA4    // ACS712 via divider (your schematic shows ISENSE_ADC on PA4)
#define RPM_PIN    PA1    // TCRT5000 DO
#define VIB_PIN    PA2    // potentiometer (simulated vibration)
#define ONEWIRE_PIN PB8   // DS18B20 data
#define BUZZER     PB14

// ---- Settings ----
const int   MOTOR_PWM      = 180;     // 0-255
const float PULSES_PER_REV = 1.0;

// Divider: R1 (OUT to pin) and R2 (pin to GND), in ohms.
// Change these to match your schematic values.
const float R1 = 1000.0;
const float R2 = 2000.0;
const float DIV_RATIO = R2 / (R1 + R2);

// Fault limits (tune by testing)
const float I_LIMIT   = 1.5;    // A
const float T_LIMIT   = 50.0;   // C
const float VIB_LIMIT = 0.5;    // 0-1 from pot
const float RPM_MIN   = 1000;   // rpm (set to 0 if the simulator gives no pulses)

OneWire ow(ONEWIRE_PIN);
DallasTemperature ds(&ow);
volatile unsigned long pulses = 0;
void onPulse() { pulses++; }

float readCurrent() {
  long sum = 0;
  for (int i = 0; i < 50; i++) sum += analogRead(ACS_PIN);
  float v   = (sum / 50.0) * 3.3 / 4095.0;
  float vin = v / DIV_RATIO;              // undo the divider
  return fabs((vin - 2.5) / 0.100);       // 100 mV per A (20A module)
}

float readVibration() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(VIB_PIN);
    delay(10);                            // ~100 ms total
  }
  return (sum / 10.0) / 4095.0;           // 0.0 - 1.0
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);               // STM32duino defaults to 10 bits

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(VIB_PIN, INPUT);
  pinMode(RPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RPM_PIN), onPulse, FALLING);

  ds.begin();

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, MOTOR_PWM);
}

void loop() {
  noInterrupts(); pulses = 0; interrupts();
  unsigned long t0 = millis();

  float vib = readVibration();            // ~100 ms
  while (millis() - t0 < 1000) {}         // 1 s RPM window

  noInterrupts(); unsigned long p = pulses; interrupts();
  float rpm = p * 60.0 / PULSES_PER_REV;

  float amps = readCurrent();
  ds.requestTemperatures();
  float temp = ds.getTempCByIndex(0);

  const char* status = "OK";
  if      (amps > I_LIMIT)                      status = "OVERCURRENT";
  else if (temp > T_LIMIT)                      status = "OVERHEAT";
  else if (vib  > VIB_LIMIT)                    status = "VIBRATION";
  else if (millis() > 3000 && rpm < RPM_MIN)    status = "LOW_RPM";

  digitalWrite(BUZZER, strcmp(status, "OK") != 0);

  Serial.print(rpm);   Serial.print(",");
  Serial.print(amps);  Serial.print(",");
  Serial.print(temp);  Serial.print(",");
  Serial.print(vib);   Serial.print(",");
  Serial.println(status);
}
