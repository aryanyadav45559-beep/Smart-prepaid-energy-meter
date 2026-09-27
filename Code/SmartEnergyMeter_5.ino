#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <PZEM004Tv30.h>
#define PIN_RELAY_MAIN      4    
#define PIN_RELAY_THEFT     25   
#define PIN_RELAY_H1_ESS    26   
#define PIN_RELAY_H1_NESS   27   
#define PIN_RELAY_H2_ESS    21   
#define PIN_RELAY_H2_NESS   22   
#define PIN_ACS1            34   
#define PIN_ACS2            35   


#define PIN_BUZZER          32
#define PIN_PZEM1_RX        16   
#define PIN_PZEM1_TX        17   
#define PIN_PZEM2_RX        13   
#define PIN_PZEM2_TX        14   
#define TFT_CS              15   
#define TFT_DC              33   
#define TFT_RST              5   
#define WIFI_STA_SSID       "DEEMACHINE_LAB"     
#define WIFI_STA_PASSWORD   "43219876"  
#define WIFI_CONNECT_TIMEOUT_MS  15000UL           


#define ACS_SENSITIVITY     0.066f   
#define SUPPLY_VOLTAGE      230.0f   


#define NUM_SLOTS           8        
#define REAL_SLOT_MS        30000UL  


#define DEMO_ACCEL          5000.0f  


#define BUZZ_THEFT_MS       10000    
#define BUZZ_LOW_BAL_MS     5000     


#define INTERVAL_SENSORS_MS 2000UL   
#define INTERVAL_TFT_MS     4000UL   
#define INTERVAL_SAVE_MS    60000UL  
#define RELAY_ON            HIGH
#define RELAY_OFF           LOW
WebServer        server(80);


Adafruit_ST7735  tft   = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Preferences      prefs;
PZEM004Tv30* pzem1 = nullptr;   
PZEM004Tv30* pzem2 = nullptr;   


IPAddress staIP;
const float tariffH1[NUM_SLOTS] = { 4.0f, 5.5f, 8.0f, 6.5f, 4.0f, 8.0f, 6.5f, 5.5f };
const float tariffH2[NUM_SLOTS] = { 8.0f, 6.5f, 4.0f, 5.5f, 8.0f, 4.0f, 5.5f, 6.5f };

const char* virtualHours[NUM_SLOTS] = {
  "00:00","02:00","04:00","06:00","08:00","10:00","12:00","14:00"
};
float voltMain = 0.0f, currMain = 0.0f, powMain = 0.0f;
float voltH1   = 0.0f, currH1   = 0.0f, powH1   = 0.0f;
float currACS1 = 0.0f;   
float currACS2 = 0.0f;   
float powH2    = 0.0f;   


float balanceH1     = 100.0f;
float balanceH2     = 100.0f;
float thresholdH1   =  20.0f;
float thresholdH2   =  20.0f;
float totalDedH1    =   0.0f;
float totalDedH2    =   0.0f;
float maxBalSeen_H1 = 100.0f;  
float maxBalSeen_H2 = 100.0f;


int   tariffSlot  = 0;
float curTariffH1 = 4.0f;
float curTariffH2 = 8.0f;
unsigned long slotStartMs[NUM_SLOTS];  


bool mainRelayON  = true;
bool theftRelayON = false;
bool h1EssON      = true;
bool h1NonEssON   = true;
bool h2EssON      = true;
bool h2NonEssON   = true;


bool  theftActive  = false;
bool  mainCutDone  = false;
unsigned long theftMs = 0;


bool  buzzerON    = false;
int   buzzerDurMs = 0;
unsigned long buzzMs = 0;


bool alertH1 = false;
bool alertH2 = false;
int acs1Zero = 2048;
int acs2Zero = 2048;


uint8_t tftPage = 0;  


unsigned long lastSensorMs = 0;
unsigned long lastTariffMs = 0;
unsigned long lastTFTMs    = 0;
unsigned long lastSaveMs   = 0;
bool isValidFloat(float v, float lo, float hi) {
  return !isnan(v) && !isinf(v) && (v >= lo) && (v <= hi);
}

void saveToFlash() {
  prefs.begin("meter", false);       
  prefs.putFloat("bh1",  balanceH1);
  prefs.putFloat("bh2",  balanceH2);
  prefs.putFloat("th1",  thresholdH1);
  prefs.putFloat("th2",  thresholdH2);
  prefs.putFloat("dh1",  totalDedH1);
  prefs.putFloat("dh2",  totalDedH2);
  prefs.putFloat("mh1",  maxBalSeen_H1);
  prefs.putFloat("mh2",  maxBalSeen_H2);
  prefs.putInt(  "slot", tariffSlot);
  prefs.end();
}

void loadFromFlash() {
  prefs.begin("meter", true);        
  float v;
  
  v = prefs.getFloat("bh1", 100.0f); if (isValidFloat(v, 0.0f, 100000.0f)) balanceH1     = v;
  v = prefs.getFloat("bh2", 100.0f); if (isValidFloat(v, 0.0f, 100000.0f)) balanceH2     = v;
  v = prefs.getFloat("th1",  20.0f); if (isValidFloat(v, 0.0f,  50000.0f)) thresholdH1   = v;
  v = prefs.getFloat("th2",  20.0f); if (isValidFloat(v, 0.0f,  50000.0f)) thresholdH2   = v;
  v = prefs.getFloat("dh1",   0.0f); if (isValidFloat(v, 0.0f, 500000.0f)) totalDedH1    = v;
  v = prefs.getFloat("dh2",   0.0f); if (isValidFloat(v, 0.0f, 500000.0f)) totalDedH2    = v;
  v = prefs.getFloat("mh1", 100.0f); if (isValidFloat(v, 0.0f, 100000.0f)) maxBalSeen_H1 = v;
  v = prefs.getFloat("mh2", 100.0f); if (isValidFloat(v, 0.0f, 100000.0f)) maxBalSeen_H2 = v;
  int s = prefs.getInt("slot", 0);
  tariffSlot = (s >= 0 && s < NUM_SLOTS) ? s : 0;
  prefs.end();

  
  curTariffH1 = tariffH1[tariffSlot];
  curTariffH2 = tariffH2[tariffSlot];

  
  if (maxBalSeen_H1 < balanceH1) maxBalSeen_H1 = balanceH1;
  if (maxBalSeen_H2 < balanceH2) maxBalSeen_H2 = balanceH2;
}
#define ACS_DIVIDER_RATIO  (3.3f / 5.0f)   

float readACS_RMS(int pin) {
  
  
  const int   N       = 200;
  const int   DLY_US  = 200;
  long        sumRaw  = 0;
  long        sumSq   = 0;   

  
  int samples[200];
  for (int i = 0; i < N; i++) {
    samples[i] = analogRead(pin);
    sumRaw += samples[i];
    delayMicroseconds(DLY_US);
  }
  float mean = (float)sumRaw / N;   

  
  float varSum = 0.0f;
  for (int i = 0; i < N; i++) {
    float diff = (float)samples[i] - mean;
    varSum += diff * diff;
  }
  float rmsADC  = sqrtf(varSum / N);                      
  float current = rmsADC * (5.0f / 4095.0f) / ACS_SENSITIVITY; 

  
  if (current < 0.10f) current = 0.0f;
  return current;
}


float readACS(int pin, int zeroOffset) {
  long sum = 0;
  for (int i = 0; i < 500; i++) {
    sum += analogRead(pin);
    delayMicroseconds(50);
  }
  float avgADC  = (float)sum / 500.0f;
  float current = ((avgADC - (float)zeroOffset) * (5.0f / 4095.0f)) / ACS_SENSITIVITY;
  if (fabsf(current) < 0.08f) current = 0.0f;
  return fabsf(current);
}

void calibrateACS() {
  
  
  
  long s1 = 0, s2 = 0;
  const int CAL_N = 2000;
  for (int i = 0; i < CAL_N; i++) {
    s1 += analogRead(PIN_ACS1);
    s2 += analogRead(PIN_ACS2);
    delayMicroseconds(50);
  }
  acs1Zero = (int)(s1 / CAL_N);
  acs2Zero = (int)(s2 / CAL_N);
  Serial.printf("[ACS CAL]  ACS1_zero=%d (%.3fV)  ACS2_zero=%d (%.3fV)\n",
                acs1Zero, acs1Zero * (3.3f / 4095.0f),
                acs2Zero, acs2Zero * (3.3f / 4095.0f));
  Serial.println("[ACS CAL]  Ideal zero = 2048 counts = 1.65V at ADC (= 2.5V at ACS)");
}
void setMainRelay(bool on) {
  mainRelayON = on;
  digitalWrite(PIN_RELAY_MAIN, on ? RELAY_ON : RELAY_OFF);
}
void setTheftRelay(bool on) {
  theftRelayON = on;
  digitalWrite(PIN_RELAY_THEFT, on ? RELAY_ON : RELAY_OFF);
}
void setH1EssRelay(bool on) {
  h1EssON = on;
  digitalWrite(PIN_RELAY_H1_ESS, on ? RELAY_ON : RELAY_OFF);
}
void setH1NonEssRelay(bool on) {
  h1NonEssON = on;
  digitalWrite(PIN_RELAY_H1_NESS, on ? RELAY_ON : RELAY_OFF);
}
void setH2EssRelay(bool on) {
  h2EssON = on;
  digitalWrite(PIN_RELAY_H2_ESS, on ? RELAY_ON : RELAY_OFF);
}
void setH2NonEssRelay(bool on) {
  h2NonEssON = on;
  digitalWrite(PIN_RELAY_H2_NESS, on ? RELAY_ON : RELAY_OFF);
}
void triggerBuzzer(int durationMs) {
  buzzerON    = true;
  buzzerDurMs = durationMs;
  buzzMs      = millis();
  digitalWrite(PIN_BUZZER, HIGH);
}

void updateBuzzer() {
  if (buzzerON && (millis() - buzzMs >= (unsigned long)buzzerDurMs)) {
    buzzerON = false;
    digitalWrite(PIN_BUZZER, LOW);
  }
}
void manageLoads() {
  
  if (balanceH1 > 0.0f && balanceH1 <= thresholdH1) {
    if (h1NonEssON) setH1NonEssRelay(false);
    if (!alertH1) {
      alertH1 = true;
      if (!buzzerON) triggerBuzzer(BUZZ_LOW_BAL_MS);
      Serial.println("[H1] Balance at threshold -- H1 Non-Ess OFF");
    }
  }
  if (balanceH1 <= 0.0f) {
    balanceH1 = 0.0f;
    if (h1EssON)    setH1EssRelay(false);
    if (h1NonEssON) setH1NonEssRelay(false);
    Serial.println("[H1] Balance ZERO -- all H1 loads OFF");
  }
  if (balanceH1 > thresholdH1) alertH1 = false;  

  
  if (balanceH2 > 0.0f && balanceH2 <= thresholdH2) {
    if (h2NonEssON) setH2NonEssRelay(false);
    if (!alertH2) {
      alertH2 = true;
      if (!buzzerON) triggerBuzzer(BUZZ_LOW_BAL_MS);
      Serial.println("[H2] Balance at threshold -- H2 Non-Ess OFF");
    }
  }
  if (balanceH2 <= 0.0f) {
    balanceH2 = 0.0f;
    if (h2EssON)    setH2EssRelay(false);
    if (h2NonEssON) setH2NonEssRelay(false);
    Serial.println("[H2] Balance ZERO -- all H2 loads OFF");
  }
  if (balanceH2 > thresholdH2) alertH2 = false;
}
#define C_BG     0x0000    
#define C_HDR    0x0A29    
#define C_H1     0x07FF    
#define C_H2     0xF81F    
#define C_GREEN  0x07E0    
#define C_YELLOW 0xFFE0    
#define C_RED    0xF800    
#define C_ORANGE 0xFD20    
#define C_WHITE  0xFFFF    
#define C_GREY   0x7BEF    
void tftLine(int y, uint16_t textCol, uint16_t bgCol, const char* fmt, ...) {
  char buf[42];   
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  tft.fillRect(0, y, 160, 9, bgCol);
  tft.setTextColor(textCol, bgCol);
  tft.setTextSize(1);
  tft.setCursor(2, y + 1);
  tft.print(buf);
}


void tftPageConsumer() {
  tft.fillScreen(C_BG);

  
  tft.fillRect(0, 0, 160, 10, C_HDR);
  tft.setTextColor(C_YELLOW, C_HDR);
  tft.setTextSize(1);
  tft.setCursor(2, 1);
  tft.print("SMART METER   [PAGE 1/2]");

  
  tftLine(11, C_H1,    C_BG, "-- HOUSE 1 (PZEM2) --");
  tftLine(20, C_WHITE, C_BG, "V:%.1f I:%.2fA P:%.1fW", voltH1, currH1, powH1);

  
  uint16_t h1bc = (balanceH1 <= thresholdH1) ? C_RED : C_GREEN;
  tftLine(29, h1bc,    C_BG, "Bal:Rs%.2f @Rs%.1f/kWh", balanceH1, curTariffH1);

  
  {
    uint16_t lc = (h1EssON && h1NonEssON) ? C_GREEN :
                  (h1EssON && !h1NonEssON) ? C_YELLOW : C_RED;
    tftLine(38, lc, C_BG, "Ess:%s NEss:%s  Slt%d/8",
            h1EssON ? "ON " : "OFF",
            h1NonEssON ? "ON " : "OFF",
            tariffSlot + 1);
  }

  
  tft.drawFastHLine(0, 48, 160, C_GREY);

  
  tftLine(50, C_H2,    C_BG, "-- HOUSE 2  (ACS2)  --");
  tftLine(59, C_WHITE, C_BG, "V:230.0 I:%.2fA P:%.1fW", currACS2, powH2);

  uint16_t h2bc = (balanceH2 <= thresholdH2) ? C_RED : C_GREEN;
  tftLine(68, h2bc,    C_BG, "Bal:Rs%.2f @Rs%.1f/kWh", balanceH2, curTariffH2);

  {
    uint16_t lc = (h2EssON && h2NonEssON) ? C_GREEN :
                  (h2EssON && !h2NonEssON) ? C_YELLOW : C_RED;
    tftLine(77, lc, C_BG, "Ess:%s NEss:%s",
            h2EssON ? "ON " : "OFF",
            h2NonEssON ? "ON " : "OFF");
  }

  
  tft.drawFastHLine(0, 87, 160, C_GREY);

  
  tftLine(89,  C_ORANGE, C_BG, "ThrH1:Rs%.0f  ThrH2:Rs%.0f", thresholdH1, thresholdH2);
  tftLine(98,  C_GREY,   C_BG, "d1:Rs%.2f  d2:Rs%.2f",  totalDedH1,  totalDedH2);
  tftLine(107, C_YELLOW, C_BG, "vTime:%s  Slot:%d/8", virtualHours[tariffSlot], tariffSlot + 1);

  
  if (theftActive) {
    tft.fillRect(0, 118, 160, 10, 0x6000);
    tft.setTextColor(C_RED, 0x6000);
    tft.setTextSize(1);
    tft.setCursor(2, 119);
    tft.print("!! THEFT -- RELAY CUTS !!");
  } else {
    tft.fillRect(0, 118, 160, 10, 0x0040);
    tft.setTextColor(C_GREEN, 0x0040);
    tft.setTextSize(1);
    tft.setCursor(2, 119);
    tft.print("SECURE  |  See Page 2");
  }
}


void tftPageSystem() {
  tft.fillScreen(C_BG);

  
  tft.fillRect(0, 0, 160, 10, C_HDR);
  tft.setTextColor(C_ORANGE, C_HDR);
  tft.setTextSize(1);
  tft.setCursor(2, 1);
  tft.print("SYS VIEW   [PAGE 2/2]");

  
  tftLine(11, C_GREEN,  C_BG, "-- MAIN SUPPLY (PZEM1)--");
  tftLine(20, C_WHITE,  C_BG, "V:%.1f I:%.2fA P:%.1fW", voltMain, currMain, powMain);

  
  tftLine(29, C_YELLOW, C_BG, "ACS1(Theft): %.2f A", currACS1);
  tftLine(38, C_YELLOW, C_BG, "ACS2(H2):    %.2f A", currACS2);

  
  tftLine(47, C_GREY,   C_BG, "Xchk:%.1fW  ACS2:%.1fW",
          powMain - powH1, powH2);

  
  tft.drawFastHLine(0, 57, 160, C_GREY);

  
  tftLine(59, C_WHITE,  C_BG, "Main:%s  Theft:%s",
          mainRelayON  ? "ON " : "OFF",
          theftRelayON ? "ON " : "OFF");

  tftLine(68, C_H1,     C_BG, "H1 Ess:%s NEss:%s",
          h1EssON    ? "ON " : "OFF",
          h1NonEssON ? "ON " : "OFF");

  tftLine(77, C_H2,     C_BG, "H2 Ess:%s NEss:%s",
          h2EssON    ? "ON " : "OFF",
          h2NonEssON ? "ON " : "OFF");

  
  tft.drawFastHLine(0, 87, 160, C_GREY);

  
  tftLine(89,  C_YELLOW, C_BG, "H1:Rs%.1f/kWh H2:Rs%.1f/kWh", curTariffH1, curTariffH2);

  {
    unsigned long elapsed2 = millis() - lastTariffMs;
    long rem = (elapsed2 >= REAL_SLOT_MS) ? 0L
               : (long)((REAL_SLOT_MS - elapsed2) / 1000UL);
    tftLine(98, C_ORANGE, C_BG, "Slot:%d/8  Next in %lds", tariffSlot + 1, rem);
  }

  
  {
    char ipBuf[22];
    snprintf(ipBuf, sizeof(ipBuf), "IP:%s", staIP.toString().c_str());
    tftLine(107, C_H1,    C_BG, "%s", ipBuf);
  }

  
  if (theftActive) {
    tft.fillRect(0, 118, 160, 10, 0x6000);
    tft.setTextColor(C_RED, 0x6000);
    tft.setTextSize(1);
    tft.setCursor(2, 119);
    tft.print("!! THEFT SIMULATED !!");
  } else {
    tft.fillRect(0, 118, 160, 10, 0x0040);
    tft.setTextColor(C_GREEN, 0x0040);
    tft.setTextSize(1);
    tft.setCursor(2, 119);
    tft.print("Secure | VJTI Project");
  }
}


void updateTFT() {
  if (tftPage == 0) {
    tftPageConsumer();
    tftPage = 1;
  } else {
    tftPageSystem();
    tftPage = 0;
  }
}


void tftSplash() {
  tft.fillScreen(C_BG);
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(6, 10);  tft.println("Smart Prepaid");
  tft.setCursor(14, 34); tft.println("Energy Meter");
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(38, 62); tft.println("Initializing...");
  tft.setTextColor(C_WHITE);
  tft.setCursor(14, 76); tft.println("VJTI Academic Project");
  tft.setTextColor(C_YELLOW);
  tft.setCursor(4,  92); tft.println("Connecting to WiFi...");
  tft.setTextColor(C_GREY);
  tft.setCursor(4, 104); tft.println("IP shown on Page 2 of");
  tft.setCursor(4, 116); tft.println("TFT after connection.");
}
void readSensorsAndBill() {
  unsigned long nowMs = millis();
  float dtHours = (float)(nowMs - lastSensorMs) / 3600000.0f;
  lastSensorMs  = nowMs;

  
  
  
  
  bool pzem1Ok = false;
  if (pzem1 != nullptr) {
    float v = pzem1->voltage();
    float i = pzem1->current();
    float p = pzem1->power();
    Serial.printf("[PZEM1] raw: V=%.2f  I=%.3f  P=%.2f  %s\n",
                  v, i, p, (isnan(v) ? "<<< NaN = AC not connected to PZEM1!" : "OK"));
    if (!isnan(v) && v > 10.0f) { voltMain = v; pzem1Ok = true; }
    else                          voltMain = 0.0f;   
    if (!isnan(i) && i >= 0.0f)   currMain = i;
    else                           currMain = 0.0f;
    if (!isnan(p) && p >= 0.0f)   powMain  = p;
    else if (pzem1Ok)              powMain  = voltMain * currMain;
    else                           powMain  = 0.0f;
  }

  
  bool pzem2Ok = false;
  if (pzem2 != nullptr) {
    float v = pzem2->voltage();
    float i = pzem2->current();
    float p = pzem2->power();
    Serial.printf("[PZEM2] raw: V=%.2f  I=%.3f  P=%.2f  %s\n",
                  v, i, p, (isnan(v) ? "<<< NaN = AC not connected to PZEM2!" : "OK"));
    if (!isnan(v) && v > 10.0f) { voltH1 = v; pzem2Ok = true; }
    else                          voltH1 = 0.0f;
    if (!isnan(i) && i >= 0.0f)   currH1 = i;
    else                           currH1 = 0.0f;
    if (!isnan(p) && p >= 0.0f)   powH1  = p;
    else if (pzem2Ok)              powH1  = voltH1 * currH1;
    else                           powH1  = 0.0f;
  }

  
  
  
  currACS1 = readACS_RMS(PIN_ACS1);   
  currACS2 = readACS_RMS(PIN_ACS2);   
  powH2    = currACS2 * SUPPLY_VOLTAGE;

  Serial.printf("[ACS]   ACS1=%.3fA  ACS2=%.3fA (%.1fW)\n",
                currACS1, currACS2, powH2);
  Serial.printf("[BILL]  H1=%.1fW  H2=%.1fW  Bal1=%.2f  Bal2=%.2f\n",
                powH1, powH2, balanceH1, balanceH2);

  
  float engH1_kWh = (powH1 * dtHours * DEMO_ACCEL) / 1000.0f;
  float engH2_kWh = (powH2 * dtHours * DEMO_ACCEL) / 1000.0f;
  float dedH1     = engH1_kWh * curTariffH1;
  float dedH2     = engH2_kWh * curTariffH2;

  if (balanceH1 > 0.0f) {
    balanceH1  -= dedH1;
    totalDedH1 += dedH1;
    if (balanceH1 < 0.0f) balanceH1 = 0.0f;
  }
  if (balanceH2 > 0.0f) {
    balanceH2  -= dedH2;
    totalDedH2 += dedH2;
    if (balanceH2 < 0.0f) balanceH2 = 0.0f;
  }

  if (balanceH1 > maxBalSeen_H1) maxBalSeen_H1 = balanceH1;
  if (balanceH2 > maxBalSeen_H2) maxBalSeen_H2 = balanceH2;

  manageLoads();
}
void updateTariff() {
  tariffSlot  = (tariffSlot + 1) % NUM_SLOTS;
  curTariffH1 = tariffH1[tariffSlot];
  curTariffH2 = tariffH2[tariffSlot];
  slotStartMs[tariffSlot] = millis();
  lastTariffMs = millis();
  saveToFlash();  
  Serial.printf("[TARIFF] Slot %d  H1=Rs%.1f  H2=Rs%.1f\n",
                tariffSlot + 1, curTariffH1, curTariffH2);
}
void handleTheftLogic() {
  if (!theftActive) return;
  if (!mainCutDone && (millis() - theftMs >= 5000UL)) {
    mainCutDone = true;
    setMainRelay(false);
    Serial.println("[THEFT] Main relay CUT (5s elapsed)");
  }
}
void handleData() {
  
  unsigned long elapsedMs = millis() - lastTariffMs;
  long nextSlotSec = (elapsedMs >= REAL_SLOT_MS)
                     ? 0L
                     : (long)((REAL_SLOT_MS - elapsedMs) / 1000UL);

  
  String slotArr = "[";
  for (int i = 0; i < NUM_SLOTS; i++) {
    slotArr += String(slotStartMs[i] / 1000UL);
    if (i < NUM_SLOTS - 1) slotArr += ",";
  }
  slotArr += "]";

  String j = "{";
  
  j += "\"mainV\":"    + String(voltMain, 1) + ",";
  j += "\"mainI\":"    + String(currMain, 2) + ",";
  j += "\"mainP\":"    + String(powMain,  1) + ",";
  j += "\"acs1I\":"    + String(currACS1, 2) + ",";
  
  j += "\"h1V\":"      + String(voltH1, 1)  + ",";
  j += "\"h1I\":"      + String(currH1, 2)  + ",";
  j += "\"h1P\":"      + String(powH1,  1)  + ",";
  
  j += "\"acs2I\":"    + String(currACS2, 2) + ",";
  j += "\"h2P\":"      + String(powH2,   1)  + ",";
  j += "\"h2Cross\":"  + String(powMain - powH1, 1) + ",";
  
  j += "\"balH1\":"    + String(balanceH1,   2) + ",";
  j += "\"thrH1\":"    + String(thresholdH1, 1) + ",";
  j += "\"tarH1\":"    + String(curTariffH1, 1) + ",";
  j += "\"dedH1\":"    + String(totalDedH1,  2) + ",";
  j += "\"maxH1\":"    + String(maxBalSeen_H1, 0) + ",";
  
  j += "\"balH2\":"    + String(balanceH2,   2) + ",";
  j += "\"thrH2\":"    + String(thresholdH2, 1) + ",";
  j += "\"tarH2\":"    + String(curTariffH2, 1) + ",";
  j += "\"dedH2\":"    + String(totalDedH2,  2) + ",";
  j += "\"maxH2\":"    + String(maxBalSeen_H2, 0) + ",";
  
  j += "\"slot\":"     + String(tariffSlot)  + ",";
  j += "\"nextSlot\":" + String(nextSlotSec) + ",";
  j += "\"slotTimes\":" + slotArr            + ",";
  
  j += "\"h1Ess\":"    + String(h1EssON     ? "true":"false") + ",";
  j += "\"h1Ness\":"   + String(h1NonEssON  ? "true":"false") + ",";
  j += "\"h2Ess\":"    + String(h2EssON     ? "true":"false") + ",";
  j += "\"h2Ness\":"   + String(h2NonEssON  ? "true":"false") + ",";
  j += "\"mainR\":"    + String(mainRelayON  ? "true":"false") + ",";
  j += "\"theft\":"    + String(theftActive  ? "true":"false");
  j += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", j);
}
void handleToggle() {
  String load = server.arg("load");
  if      (load == "h1essential")    setH1EssRelay(!h1EssON);
  else if (load == "h1nonessential") setH1NonEssRelay(!h1NonEssON);
  else if (load == "h2essential")    setH2EssRelay(!h2EssON);
  else if (load == "h2nonessential") setH2NonEssRelay(!h2NonEssON);
  else { server.send(400, "text/plain", "Unknown load"); return; }
  server.send(200, "text/plain", "OK");
}
void handleRecharge() {
  int   house = server.arg("house").toInt();
  float amt   = server.arg("amount").toFloat();
  if (amt <= 0.0f || house < 1 || house > 2) {
    server.send(400, "text/plain", "Invalid params");
    return;
  }
  if (house == 1) {
    balanceH1 += amt;
    if (balanceH1 > maxBalSeen_H1) maxBalSeen_H1 = balanceH1;
    alertH1 = false;
    
    if (balanceH1 > thresholdH1 && !theftActive) {
      setH1EssRelay(true);
      setH1NonEssRelay(true);
    }
    Serial.printf("[RECHARGE] H1 +Rs%.2f -> Rs%.2f\n", amt, balanceH1);
  } else {
    balanceH2 += amt;
    if (balanceH2 > maxBalSeen_H2) maxBalSeen_H2 = balanceH2;
    alertH2 = false;
    
    if (balanceH2 > thresholdH2) {
      setH2EssRelay(true);
      setH2NonEssRelay(true);
    }
    Serial.printf("[RECHARGE] H2 +Rs%.2f -> Rs%.2f\n", amt, balanceH2);
  }
  saveToFlash();
  server.send(200, "text/plain", "OK");
}
void handleSetThresh() {
  int   house = server.arg("house").toInt();
  float val   = server.arg("value").toFloat();
  if (val < 0.0f || house < 1 || house > 2) {
    server.send(400, "text/plain", "Invalid params");
    return;
  }
  if (house == 1) thresholdH1 = val;
  else            thresholdH2 = val;
  saveToFlash();
  Serial.printf("[THRESH] H%d set to Rs%.2f\n", house, val);
  server.send(200, "text/plain", "OK");
}
void handleTheft() {
  if (server.arg("action") == "trigger" && !theftActive) {
    theftActive  = true;
    mainCutDone  = false;
    theftMs      = millis();
    setTheftRelay(true);
    triggerBuzzer(BUZZ_THEFT_MS);
    Serial.println("[THEFT] Simulation TRIGGERED -- Buzzer 10s, Main cuts in 5s");
  }
  server.send(200, "text/plain", "OK");
}
void handleReset() {
  
  
  prefs.begin("meter", false);
  prefs.clear();      
  prefs.end();

  
  theftActive   = false;
  mainCutDone   = false;
  alertH1       = false;
  alertH2       = false;
  balanceH1     = 100.0f;
  balanceH2     = 100.0f;
  thresholdH1   =  20.0f;   
  thresholdH2   =  20.0f;
  totalDedH1    =   0.0f;
  totalDedH2    =   0.0f;
  maxBalSeen_H1 = 100.0f;
  maxBalSeen_H2 = 100.0f;
  tariffSlot    = 0;
  curTariffH1   = tariffH1[0];
  curTariffH2   = tariffH2[0];
  lastTariffMs  = millis();
  memset(slotStartMs, 0, sizeof(slotStartMs));
  slotStartMs[0] = millis();

  
  setTheftRelay(false);
  setMainRelay(true);
  setH1EssRelay(true);
  setH1NonEssRelay(true);
  setH2EssRelay(true);
  setH2NonEssRelay(true);

  
  buzzerON = false;
  digitalWrite(PIN_BUZZER, LOW);

  
  saveToFlash();

  Serial.println("[RESET] Full factory reset -- NVS cleared, all defaults restored.");
  server.send(200, "text/plain", "OK");
}
const char HTML_PAGE[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Prepaid Energy Meter</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh}
.hdr{background:linear-gradient(135deg,#0f2744,#1f6feb);padding:12px 18px;
     display:flex;align-items:center;justify-content:space-between;box-shadow:0 2px 14px #0009}
.hdr h1{font-size:1.1rem;font-weight:700}
.hdr .sub{font-size:.7rem;color:#93b8de;margin-top:2px}
.dot{width:8px;height:8px;border-radius:50%;background:#3fb950;display:inline-block;
     margin-right:5px;animation:bk 1.4s infinite}
@keyframes bk{0%,100%{opacity:1}50%{opacity:.2}}
.tabs{display:flex;background:#161b22;border-bottom:2px solid #21262d}
.tab{padding:10px 24px;cursor:pointer;font-size:.86rem;border-bottom:3px solid transparent;
     color:#8b949e;transition:.18s;user-select:none;font-weight:600}
.tab.active{color:#58a6ff;border-bottom-color:#58a6ff}
.tab:hover{background:#1c2128;color:#e6edf3}
.panel{display:none;padding:16px}.panel.show{display:block}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px}
@media(max-width:700px){.row2{grid-template-columns:1fr}}
.card{background:#161b22;border:1px solid #30363d;border-radius:11px;padding:15px}
.card-title{font-size:.9rem;font-weight:700;margin-bottom:12px;display:flex;align-items:center;gap:6px}
.h1c .card-title{color:#58a6ff}.h2c .card-title{color:#bc8cff}
.metrics{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;margin-bottom:12px}
.m4{grid-template-columns:repeat(4,1fr)}
.met{background:#0d1117;border:1px solid #21262d;border-radius:8px;padding:8px;text-align:center}
.met .v{font-size:1.18rem;font-weight:700;color:#3fb950}
.met .u{font-size:.66rem;color:#8b949e}.met .l{font-size:.68rem;color:#8b949e;margin-top:1px}
.balbox{background:#0d1117;border-radius:8px;padding:10px;margin-bottom:10px}
.balrow{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
.bal-num{font-size:1.4rem;font-weight:800}.bal-lbl{font-size:.7rem;color:#8b949e}
.tbadge{background:#1c2e48;border:1px solid #1f6feb;color:#58a6ff;
        padding:3px 8px;border-radius:13px;font-size:.76rem;font-weight:700}
.pbar{background:#21262d;border-radius:4px;height:5px;margin-top:4px}
.pfill{height:100%;border-radius:4px;background:#3fb950;transition:width .5s ease}
.pfill.mid{background:#d29922}.pfill.low{background:#f85149}
.bal-sub{display:flex;justify-content:space-between;font-size:.67rem;color:#8b949e;margin-top:3px}
.lbadges{display:flex;gap:7px;flex-wrap:wrap;margin-bottom:10px}
.lbadge{padding:4px 10px;border-radius:10px;font-size:.73rem;font-weight:700;cursor:pointer;
        transition:.15s;user-select:none}
.lon{background:#1a4731;color:#3fb950;border:1px solid #238636}
.loff{background:#3d1214;color:#f85149;border:1px solid #da3633}
.lbadge:hover{opacity:.8}
.ctrls{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:9px}
.btn{padding:7px 12px;border:none;border-radius:6px;cursor:pointer;
     font-size:.79rem;font-weight:600;transition:.14s}
.gr{background:#1a4731;color:#3fb950;border:1px solid #238636}
.gr:hover{background:#238636;color:#fff}
.rd{background:#3d1214;color:#f85149;border:1px solid #da3633}
.rd:hover{background:#da3633;color:#fff}
.bl{background:#0c2740;color:#58a6ff;border:1px solid #1f6feb}
.bl:hover{background:#1f6feb;color:#fff}
.or{background:#3a2200;color:#d29922;border:1px solid #9e6a03}
.or:hover{background:#9e6a03;color:#fff}
.gy{background:#21262d;color:#8b949e;border:1px solid #30363d}
.gy:hover{background:#30363d;color:#e6edf3}
.pu{background:#2d1f47;color:#bc8cff;border:1px solid #7c3aed}
.pu:hover{background:#7c3aed;color:#fff}
.irow{display:flex;gap:6px;align-items:center;margin-bottom:7px}
.inp{background:#0d1117;border:1px solid #30363d;color:#e6edf3;
     padding:6px 9px;border-radius:6px;font-size:.82rem;width:115px}
.inp:focus{outline:none;border-color:#58a6ff}
.info-bar{background:#1c2a1c;border:1px solid #238636;border-radius:8px;
          padding:8px 13px;font-size:.77rem;color:#7ee787;margin-bottom:13px}
.warn-bar{background:#2a1c1c;border:1px solid #da3633;border-radius:8px;
          padding:8px 13px;font-size:.77rem;color:#ff7b72;display:none;margin-top:9px}
table{width:100%;border-collapse:collapse;font-size:.79rem}
th{background:#21262d;padding:6px 5px;text-align:center;color:#8b949e;font-weight:600}
td{padding:5px;text-align:center;border-bottom:1px solid #1a1f26}
tr.act td{background:#162416}
.pk{color:#f85149;font-weight:700}.op{color:#3fb950;font-weight:700}.mp{color:#d29922;font-weight:700}
.sumrow{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:14px}
@media(max-width:700px){.sumrow{grid-template-columns:1fr 1fr}}
.sum-item{background:#0d1117;border-radius:8px;padding:10px;text-align:center;border:1px solid #21262d}
.sum-val{font-size:1.35rem;font-weight:800;color:#d29922}
.sum-lbl{font-size:.68rem;color:#8b949e;margin-top:2px}
.relay-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:13px}
@media(max-width:500px){.relay-grid{grid-template-columns:1fr 1fr}}
.relay-card{background:#0d1117;border:1px solid #21262d;border-radius:8px;
            padding:10px;text-align:center}
.relay-card .rv{font-size:1rem;font-weight:800}
.relay-card .rl{font-size:.68rem;color:#8b949e;margin-top:2px}
.theft-dot{width:13px;height:13px;border-radius:50%;display:inline-block;margin-right:7px}
.t-safe{background:#3fb950}.t-act{background:#f85149;animation:bk .7s infinite}
.theft-row{display:flex;align-items:center;margin-bottom:11px;font-weight:600;font-size:.9rem}
.tval{font-size:1.3rem;font-weight:800;color:#d29922}
</style>
</head>
<body>

<div class="hdr">
  <div>
    <h1>&#9889; Smart Prepaid Energy Meter</h1>
    <div class="sub">ESP32 &middot; 2&times;PZEM-004T &middot; 2&times;ACS712 &middot; 6 Relays &middot; VJTI Prototype</div>
  </div>
  <div style="font-size:.8rem;text-align:right">
    <span class="dot"></span><span id="cs">Connecting&hellip;</span><br>
    <span style="font-size:.68rem;color:#58a6ff" id="devip">see Serial Monitor for IP</span>
  </div>
</div>

<div class="tabs">
  <div class="tab active" onclick="sw('consumer',this)">&#127968; Consumer</div>
  <div class="tab"        onclick="sw('grid',this)">&#9889; Grid Operator</div>
</div>

<!-- ==================== CONSUMER TAB ==================== -->
<div id="consumer" class="panel show">
  <div class="row2">

    <!-- House 1 -->
    <div class="card h1c">
      <div class="card-title">&#127968; House 1
        <span style="font-size:.68rem;color:#8b949e;font-weight:400">(PZEM-004T #2)</span>
      </div>
      <div class="metrics">
        <div class="met"><div class="v" id="h1v">--</div><div class="u">V</div><div class="l">Voltage</div></div>
        <div class="met"><div class="v" id="h1i">--</div><div class="u">A</div><div class="l">Current</div></div>
        <div class="met"><div class="v" id="h1p">--</div><div class="u">W</div><div class="l">Power</div></div>
      </div>
      <div class="balbox">
        <div class="balrow">
          <div><div class="bal-lbl">Wallet Balance</div>
            <div class="bal-num" id="h1bal">&#8377;--</div></div>
          <div style="text-align:right">
            <div class="tbadge">&#8377;<span id="h1tar">-</span>/kWh</div>
            <div style="font-size:.67rem;color:#8b949e;margin-top:3px">
              Slot <span id="h1slot">-</span>/8 &nbsp;&middot;&nbsp; <span id="h1vt">--:--</span>
            </div>
          </div>
        </div>
        <div class="pbar"><div class="pfill" id="h1pb" style="width:100%"></div></div>
        <div class="bal-sub">
          <span>Deducted: &#8377;<span id="h1ded">0.00</span></span>
          <span>Threshold: &#8377;<span id="h1thr">--</span></span>
        </div>
      </div>
      <div style="font-size:.69rem;color:#8b949e;margin-bottom:4px">Load status (tap badge to toggle):</div>
      <div class="lbadges">
        <span class="lbadge lon" id="h1essB" onclick="tog('h1essential')">&#9889; H1 Essential: --</span>
        <span class="lbadge lon" id="h1nesB" onclick="tog('h1nonessential')">&#128161; H1 Non-Ess: --</span>
      </div>
      <div class="ctrls">
        <button class="btn gr" onclick="tog('h1essential')">&#9889; Toggle H1 Ess</button>
        <button class="btn or" onclick="tog('h1nonessential')">&#128161; Toggle H1 Non-Ess</button>
      </div>
      <div class="irow">
        <input type="number" class="inp" id="rH1" placeholder="Amount &#8377;" min="1" step="10">
        <button class="btn bl" onclick="rech(1)">&#128179; Recharge H1</button>
      </div>
      <div class="irow">
        <input type="number" class="inp" id="tH1" placeholder="Threshold &#8377;" min="0">
        <button class="btn gy" onclick="setThr(1)">&#127919; Set H1 Threshold</button>
      </div>
    </div>

    <!-- House 2 -->
    <div class="card h2c">
      <div class="card-title">&#127968; House 2
        <span style="font-size:.68rem;color:#8b949e;font-weight:400">(ACS712 #2)</span>
      </div>
      <div class="metrics">
        <div class="met"><div class="v">230</div><div class="u">V</div><div class="l">Ref Voltage</div></div>
        <div class="met"><div class="v" id="h2i">--</div><div class="u">A</div><div class="l">ACS2 Current</div></div>
        <div class="met"><div class="v" id="h2p">--</div><div class="u">W</div><div class="l">Power</div></div>
      </div>
      <div style="background:#0d1117;border-radius:7px;padding:6px 9px;
                  font-size:.69rem;color:#8b949e;margin-bottom:10px">
        Cross-check (Main&minus;H1):
        <span id="h2xc" style="color:#d29922;font-weight:700">-- W</span>
        &nbsp;&asymp;&nbsp; ACS2 power:
        <span id="h2ac" style="color:#bc8cff;font-weight:700">-- W</span>
      </div>
      <div class="balbox">
        <div class="balrow">
          <div><div class="bal-lbl">Wallet Balance</div>
            <div class="bal-num" id="h2bal">&#8377;--</div></div>
          <div style="text-align:right">
            <div class="tbadge">&#8377;<span id="h2tar">-</span>/kWh</div>
            <div style="font-size:.67rem;color:#8b949e;margin-top:3px">
              Slot <span id="h2slot">-</span>/8 &nbsp;&middot;&nbsp; <span id="h2vt">--:--</span>
            </div>
          </div>
        </div>
        <div class="pbar"><div class="pfill" id="h2pb" style="width:100%"></div></div>
        <div class="bal-sub">
          <span>Deducted: &#8377;<span id="h2ded">0.00</span></span>
          <span>Threshold: &#8377;<span id="h2thr">--</span></span>
        </div>
      </div>
      <div style="font-size:.69rem;color:#8b949e;margin-bottom:4px">Load status (tap badge to toggle):</div>
      <div class="lbadges">
        <span class="lbadge lon" id="h2essB" onclick="tog('h2essential')">&#9889; H2 Essential: --</span>
        <span class="lbadge lon" id="h2nesB" onclick="tog('h2nonessential')">&#128161; H2 Non-Ess: --</span>
      </div>
      <div class="ctrls">
        <button class="btn pu" onclick="tog('h2essential')">&#9889; Toggle H2 Ess</button>
        <button class="btn or" onclick="tog('h2nonessential')">&#128161; Toggle H2 Non-Ess</button>
      </div>
      <div class="irow">
        <input type="number" class="inp" id="rH2" placeholder="Amount &#8377;" min="1" step="10">
        <button class="btn bl" onclick="rech(2)">&#128179; Recharge H2</button>
      </div>
      <div class="irow">
        <input type="number" class="inp" id="tH2" placeholder="Threshold &#8377;" min="0">
        <button class="btn gy" onclick="setThr(2)">&#127919; Set H2 Threshold</button>
      </div>
    </div>

  </div><!-- end row2 -->

  <!-- Main Supply Card -->
  <div class="card" style="margin-bottom:0">
    <div class="card-title" style="color:#3fb950">&#128202; Main Supply &mdash; PZEM1</div>
    <div class="metrics m4">
      <div class="met"><div class="v" id="mV">--</div><div class="u">V</div><div class="l">Voltage</div></div>
      <div class="met"><div class="v" id="mI">--</div><div class="u">A</div><div class="l">Current</div></div>
      <div class="met"><div class="v" id="mP">--</div><div class="u">W</div><div class="l">Power</div></div>
      <div class="met"><div class="v" id="a1I" style="color:#f85149">--</div>
           <div class="u">A</div><div class="l">ACS1 (Theft)</div></div>
    </div>
  </div>
</div><!-- end consumer -->

<!-- ==================== GRID OPERATOR TAB ==================== -->
<div id="grid" class="panel">

  <div class="info-bar">
    &#128203; <b>Grid Stability:</b> H1 &amp; H2 tariffs are phase-offset.
    H1 peak = H2 off-peak and vice versa &mdash; this redistributes load, flattening demand.
    <b>Mean H1 = Mean H2 = &#8377;6.00/kWh</b> (sum=48, slots=8, 48&divide;8=6.00 &#10003;).
  </div>

  <div class="sumrow">
    <div class="sum-item"><div class="sum-lbl">H1 Mean Tariff</div>
      <div class="sum-val">&#8377;6.00</div><div class="sum-lbl">per kWh</div></div>
    <div class="sum-item"><div class="sum-lbl">H2 Mean Tariff</div>
      <div class="sum-val">&#8377;6.00</div><div class="sum-lbl">per kWh</div></div>
    <div class="sum-item"><div class="sum-lbl">Active Slot</div>
      <div class="sum-val" id="gSlot">-</div><div class="sum-lbl">of 8</div></div>
    <div class="sum-item"><div class="sum-lbl">Next Slot In</div>
      <div class="sum-val tval" id="gTimer">--</div><div class="sum-lbl">real seconds</div></div>
  </div>

  <div class="row2">
    <div class="card">
      <div class="card-title" style="color:#58a6ff">&#127968; House 1 &mdash; Tariff Schedule</div>
      <table>
        <thead><tr><th>Slot</th><th>Virtual Time</th><th>Real Clock</th><th>Rate &#8377;/kWh</th></tr></thead>
        <tbody id="t1body"></tbody>
      </table>
      <div style="font-size:.67rem;color:#8b949e;margin-top:7px">
        30s real = 2h virtual &nbsp;|&nbsp;
        <span style="color:#f85149">&#9632; Peak</span>&nbsp;
        <span style="color:#d29922">&#9632; Mid</span>&nbsp;
        <span style="color:#3fb950">&#9632; Off-peak</span>
      </div>
    </div>
    <div class="card">
      <div class="card-title" style="color:#bc8cff">&#127968; House 2 &mdash; Tariff Schedule</div>
      <table>
        <thead><tr><th>Slot</th><th>Virtual Time</th><th>Real Clock</th><th>Rate &#8377;/kWh</th></tr></thead>
        <tbody id="t2body"></tbody>
      </table>
      <div style="font-size:.67rem;color:#8b949e;margin-top:7px">
        Offset from H1 &mdash; load shifts between houses each slot &#10003;
      </div>
    </div>
  </div>

  <!-- All Relay Status Panel -->
  <div class="card" style="margin-bottom:14px">
    <div class="card-title" style="color:#d29922">&#128268; All Relay / Load Status</div>
    <div class="relay-grid">
      <div class="relay-card">
        <div class="rv" id="rs_main" style="color:#3fb950">ON</div>
        <div class="rl">Main Supply</div>
      </div>
      <div class="relay-card">
        <div class="rv" id="rs_theft" style="color:#3fb950">OFF</div>
        <div class="rl">Theft Relay</div>
      </div>
      <div class="relay-card">
        <div class="rv" id="rs_h1e" style="color:#3fb950">ON</div>
        <div class="rl">H1 Essential</div>
      </div>
      <div class="relay-card">
        <div class="rv" id="rs_h1n" style="color:#3fb950">ON</div>
        <div class="rl">H1 Non-Ess</div>
      </div>
      <div class="relay-card">
        <div class="rv" id="rs_h2e" style="color:#3fb950">ON</div>
        <div class="rl">H2 Essential</div>
      </div>
      <div class="relay-card">
        <div class="rv" id="rs_h2n" style="color:#3fb950">ON</div>
        <div class="rl">H2 Non-Ess</div>
      </div>
    </div>
  </div>

  <!-- Theft + Reset -->
  <div class="card">
    <div class="card-title" style="color:#f85149">&#9888; Theft Simulation &amp; System Control</div>
    <div class="theft-row">
      <span class="theft-dot t-safe" id="tDot"></span>
      <span id="tStat">&#10003; System Secure</span>
    </div>
    <p style="font-size:.77rem;color:#8b949e;margin-bottom:12px">
      Sequence: Theft relay ON &rarr; ACS1 detects current surge &rarr; Buzzer 10s &rarr;
      Main relay cuts after 5s. Use System Reset to restore full operation.
    </p>
    <div class="ctrls">
      <button class="btn rd" onclick="doTheft()">&#9889; Simulate Theft</button>
      <button class="btn gr" onclick="doReset()">&#128260; Full System Reset</button>
      <button class="btn or" onclick="doRecal()">&#127919; Recalibrate ACS (no-load)</button>
    </div>
    <div class="warn-bar" id="warnBar">
      &#128680; THEFT DETECTED &mdash; Main relay cutting in 5s! Buzzer active 10s.
      ACS1 showing abnormal current surge.
    </div>
  </div>

</div><!-- end grid -->

<script>
const TH1=[4.0,5.5,8.0,6.5,4.0,8.0,6.5,5.5];
const TH2=[8.0,6.5,4.0,5.5,8.0,4.0,5.5,6.5];
const VHR=['00:00','02:00','04:00','06:00','08:00','10:00','12:00','14:00'];
let slotTimes=new Array(8).fill(0), lastSlot=0, errCount=0;

function sw(id,el){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('show'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.getElementById(id).classList.add('show');
  el.classList.add('active');
  if(id==='grid') buildTables(lastSlot);
}
function tc(r){return r>=7.5?'pk':r<=4.5?'op':'mp';}
function buildTables(s){
  [['t1body',TH1],['t2body',TH2]].forEach(([id,T])=>{
    let h='';
    T.forEach((r,i)=>{
      const rt=slotTimes[i]>0?sHMS(slotTimes[i]):'--:--:--';
      h+=`<tr class="${i===s?'act':''}">
        <td>${i+1}</td><td>${VHR[i]}</td><td>${rt}</td>
        <td class="${tc(r)}">&#8377;${r.toFixed(1)}</td></tr>`;
    });
    document.getElementById(id).innerHTML=h;
  });
}
function sHMS(s){return[Math.floor(s/3600),Math.floor((s%3600)/60),s%60].map(n=>String(n).padStart(2,'0')).join(':');}
function setBar(id,v,mx){
  const p=Math.min(100,Math.max(0,(v/mx)*100));
  const el=document.getElementById(id);
  el.style.width=p+'%';
  el.className='pfill'+(p<20?' low':p<50?' mid':'');
}
function setBadge(id,on,lbl){
  const el=document.getElementById(id);
  el.innerHTML=lbl+': '+(on?'ON':'OFF');
  el.className='lbadge '+(on?'lon':'loff');
}
function setRelay(id,on){
  const el=document.getElementById(id);
  el.textContent=on?'ON':'OFF';
  el.style.color=on?'#3fb950':'#f85149';
}
function sv(id,val,d){document.getElementById(id).textContent=(d!==undefined)?val.toFixed(d):val;}
let errC=0;
function fetchData(){
  fetch('/data').then(r=>r.json()).then(d=>{
    errC=0;
    document.getElementById('cs').textContent='Connected';
    sv('mV',d.mainV,1);sv('mI',d.mainI,2);sv('mP',d.mainP,1);sv('a1I',d.acs1I,2);
    sv('h1v',d.h1V,1);sv('h1i',d.h1I,2);sv('h1p',d.h1P,1);
    document.getElementById('h1bal').innerHTML='&#8377;'+d.balH1.toFixed(2);
    sv('h1tar',d.tarH1,1);sv('h1slot',d.slot+1);
    document.getElementById('h1vt').textContent=VHR[d.slot];
    sv('h1ded',d.dedH1,2);sv('h1thr',d.thrH1,1);
    setBar('h1pb',d.balH1,d.maxH1);
    setBadge('h1essB',d.h1Ess,'&#9889; H1 Essential');
    setBadge('h1nesB',d.h1Ness,'&#128161; H1 Non-Ess');
    sv('h2i',d.acs2I,2);sv('h2p',d.h2P,1);
    document.getElementById('h2xc').textContent=d.h2Cross.toFixed(1)+' W';
    document.getElementById('h2ac').textContent=d.h2P.toFixed(1)+' W';
    document.getElementById('h2bal').innerHTML='&#8377;'+d.balH2.toFixed(2);
    sv('h2tar',d.tarH2,1);sv('h2slot',d.slot+1);
    document.getElementById('h2vt').textContent=VHR[d.slot];
    sv('h2ded',d.dedH2,2);sv('h2thr',d.thrH2,1);
    setBar('h2pb',d.balH2,d.maxH2);
    setBadge('h2essB',d.h2Ess,'&#9889; H2 Essential');
    setBadge('h2nesB',d.h2Ness,'&#128161; H2 Non-Ess');
    lastSlot=d.slot;
    slotTimes=d.slotTimes||slotTimes;
    sv('gSlot',d.slot+1);sv('gTimer',d.nextSlot);
    if(document.getElementById('grid').classList.contains('show')) buildTables(d.slot);
    setRelay('rs_main',d.mainR);
    setRelay('rs_theft',d.theft);
    setRelay('rs_h1e',d.h1Ess);
    setRelay('rs_h1n',d.h1Ness);
    setRelay('rs_h2e',d.h2Ess);
    setRelay('rs_h2n',d.h2Ness);
    const t=d.theft;
    document.getElementById('tDot').className='theft-dot '+(t?'t-act':'t-safe');
    document.getElementById('tStat').textContent=t?'THEFT ACTIVE -- cutting power!':'System Secure';
    document.getElementById('warnBar').style.display=t?'block':'none';
  }).catch(()=>{if(++errC>3)document.getElementById('cs').textContent='Reconnecting...';});
}
function tog(l){fetch('/toggle?load='+l).then(fetchData);}
function rech(h){
  const v=document.getElementById('rH'+h).value;
  if(!v||parseFloat(v)<=0){alert('Enter a valid amount.');return;}
  fetch('/recharge?house='+h+'&amount='+v).then(()=>{document.getElementById('rH'+h).value='';fetchData();});
}
function setThr(h){
  const v=document.getElementById('tH'+h).value;
  if(v===''){alert('Enter threshold value.');return;}
  fetch('/setthresh?house='+h+'&value='+v).then(()=>{document.getElementById('tH'+h).value='';fetchData();});
}
function doTheft(){
  if(!confirm('Simulate theft?\n- Theft relay ON\n- Buzzer 10s\n- Main cuts in 5s'))return;
  fetch('/theft?action=trigger').then(fetchData);
}
function doReset(){
  if(!confirm('Full reset?\n- Balances to Rs100\n- All relays ON\n- Tariff slot 1\n- Deductions cleared'))return;
  fetch('/reset').then(fetchData);
}
function doRecal(){
  if(!confirm('Recalibrate ACS zero offset?\n\nDo this ONLY when:\n1. No load is connected to House-2 ACS wire\n2. Theft relay is OFF\n\nTakes ~2 seconds.'))return;
  fetch('/recalibrate').then(()=>{alert('ACS recalibrated. Check Serial Monitor for new zero values.');fetchData();});
}
buildTables(0);
fetchData();
setInterval(fetchData,1000);
</script>
</body>
</html>
)HTMLEOF";
void handleRecalibrate() {
  Serial.println("[ACS] Manual recalibration triggered from dashboard.");
  Serial.println("[ACS] Ensure ALL loads are OFF before this runs.");
  calibrateACS();
  server.send(200, "text/plain", "OK");
}

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\n============================================");
  Serial.println("   Smart Prepaid Energy Meter  --  BOOT");
  Serial.println("   VJTI Academic Project  |  ESP32 WROOM-32");
  Serial.println("============================================");

  
  
  digitalWrite(PIN_RELAY_MAIN,    RELAY_OFF);
  digitalWrite(PIN_RELAY_THEFT,   RELAY_OFF);
  digitalWrite(PIN_RELAY_H1_ESS,  RELAY_OFF);
  digitalWrite(PIN_RELAY_H1_NESS, RELAY_OFF);
  digitalWrite(PIN_RELAY_H2_ESS,  RELAY_OFF);
  digitalWrite(PIN_RELAY_H2_NESS, RELAY_OFF);

  pinMode(PIN_RELAY_MAIN,    OUTPUT);
  pinMode(PIN_RELAY_THEFT,   OUTPUT);
  pinMode(PIN_RELAY_H1_ESS,  OUTPUT);
  pinMode(PIN_RELAY_H1_NESS, OUTPUT);
  pinMode(PIN_RELAY_H2_ESS,  OUTPUT);
  pinMode(PIN_RELAY_H2_NESS, OUTPUT);

  
  setMainRelay(true);       
  setTheftRelay(false);     
  setH1EssRelay(true);      
  setH1NonEssRelay(true);   
  setH2EssRelay(true);      
  setH2NonEssRelay(true);   

  Serial.println("[GPIO] All relay pins configured and set to initial states");

  
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  
  analogReadResolution(12);           
  analogSetAttenuation(ADC_11db);     

  
  loadFromFlash();
  Serial.printf("[FLASH] Loaded: H1=Rs%.2f  H2=Rs%.2f  "
                "ThrH1=Rs%.2f  ThrH2=Rs%.2f  Slot=%d\n",
                balanceH1, balanceH2, thresholdH1, thresholdH2, tariffSlot);

  
  
  
  
  tft.initR(INITR_BLACKTAB);
  delay(150);                    
  tft.setRotation(1);            
  tft.fillScreen(0xF800);        
  delay(300);
  tft.fillScreen(0x0000);        
  tftSplash();
  Serial.println("[TFT] Adafruit ST7735 1.8\" initialised (160x128 landscape).");

  
  
  Serial.println("[ACS] Starting zero calibration -- ensure NO LOAD on ACS1/ACS2 for 2s");
  delay(2000);     
  calibrateACS();

  
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);

  
  tft.fillRect(0, 92, 160, 10, C_BG);
  tft.setTextColor(C_YELLOW, C_BG);
  tft.setTextSize(1);

  unsigned long wifiStart = millis();
  int dots = 0;
  Serial.print("[WiFi] Connecting to ");
  Serial.print(WIFI_STA_SSID);
  while (WiFi.status() != WL_CONNECTED &&
         millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
    
    char dotBuf[20];
    snprintf(dotBuf, sizeof(dotBuf), "Connecting%.*s", (dots % 4) + 1, "....");
    tft.fillRect(0, 92, 160, 10, C_BG);
    tft.setCursor(4, 93);
    tft.print(dotBuf);
    dots++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    staIP = WiFi.localIP();
    Serial.println("");
    Serial.println("  +------------------------------------------+");
    Serial.println("  |         WiFi Station Mode READY          |");
    Serial.printf( "  |  SSID     : %-28s|\n", WIFI_STA_SSID);
    Serial.printf( "  |  IP Addr  : %-28s|\n", staIP.toString().c_str());
    Serial.printf( "  |  Dashboard: http:
    Serial.println("  +------------------------------------------+");
    Serial.println("");
    
    tft.fillRect(0, 88, 160, 30, C_BG);
    tft.setTextColor(C_GREEN, C_BG);
    tft.setCursor(4, 92); tft.println("WiFi Connected!");
    tft.setTextColor(C_YELLOW, C_BG);
    tft.setCursor(4, 104);
    tft.printf("IP: %s", staIP.toString().c_str());
  } else {
    Serial.println("[WiFi] Connection FAILED. Check SSID/Password.");
    Serial.println("[WiFi] Dashboard will be unavailable.");
    staIP.fromString("0.0.0.0");
    tft.fillRect(0, 88, 160, 20, C_BG);
    tft.setTextColor(C_RED, C_BG);
    tft.setCursor(4, 92); tft.println("WiFi FAILED!");
    tft.setCursor(4, 104); tft.println("Check credentials.");
    delay(3000);
  }

  
  server.on("/",            handleRoot);
  server.on("/data",        handleData);
  server.on("/toggle",      handleToggle);
  server.on("/recharge",    handleRecharge);
  server.on("/setthresh",   handleSetThresh);
  server.on("/theft",       handleTheft);
  server.on("/reset",       handleReset);
  server.on("/recalibrate", handleRecalibrate);
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");

  
  
  
  
  
  
  
  
  
  Serial.println("[PZEM] Starting Serial2 for PZEM1 (Main)...");
  Serial2.begin(9600, SERIAL_8N1, PIN_PZEM1_RX, PIN_PZEM1_TX);
  delay(100);
  pzem1 = new PZEM004Tv30(&Serial2, PIN_PZEM1_RX, PIN_PZEM1_TX);
  delay(100);

  Serial.println("[PZEM] Starting Serial1 for PZEM2 (House-1)...");
  Serial1.begin(9600, SERIAL_8N1, PIN_PZEM2_RX, PIN_PZEM2_TX);
  delay(100);
  pzem2 = new PZEM004Tv30(&Serial1, PIN_PZEM2_RX, PIN_PZEM2_TX);
  delay(100);

  Serial.println("[PZEM] Both sensors initialised.");
  Serial.println("[PZEM] Expected: LED on each PZEM blinks once per second.");
  Serial.println("[PZEM] If no blink: verify PZEM TX->ESP32 RX and PZEM RX->ESP32 TX.");

  
  memset(slotStartMs, 0, sizeof(slotStartMs));
  slotStartMs[tariffSlot] = millis();  

  
  lastSensorMs = millis();
  lastTariffMs = millis();
  lastTFTMs    = millis();
  lastSaveMs   = millis();

  Serial.println("[BOOT] Setup complete -- entering main loop");
  Serial.println("================================================");
}
void loop() {
  
  server.handleClient();

  
  updateBuzzer();

  unsigned long now = millis();

  
  if (now - lastSensorMs >= INTERVAL_SENSORS_MS) {
    readSensorsAndBill();
    
  }

  
  if (now - lastTariffMs >= REAL_SLOT_MS) {
    updateTariff();
  }

  
  handleTheftLogic();

  
  if (now - lastTFTMs >= INTERVAL_TFT_MS) {
    lastTFTMs = now;
    updateTFT();
  }

  
  if (now - lastSaveMs >= INTERVAL_SAVE_MS) {
    lastSaveMs = now;
    saveToFlash();
    Serial.printf("[SAVE] Periodic flash write  H1=Rs%.2f  H2=Rs%.2f\n",
                  balanceH1, balanceH2);
  }
}

