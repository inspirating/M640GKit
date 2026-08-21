/*
================================================================================
M640GKit ±ÃÄ£ÄâÆ÷ - nRF52840 (Nice!Nano) Arduino Èë¿ÚÎÄ¼ş
================================================================================

¸Ã³ÌĞòÄ£Äâ M640G ÒÈµºËØ±Ã, ×÷Îª BLE GATT Server ÔËĞĞ,
¹© iOS Loop app / Trio »òÆäËû BLE ¿Í»§¶ËÁ¬½ÓºÍÍ¨ĞÅ¡£

ÒÆÖ²×Ô ESP32/ESP32.ino, ÒµÎñÂß¼­Áã¸Ä¶¯ (¼û pump_simulator.h)¡£
Óë ESP32 °æµÄ²îÒì (½öÈë¿ÚÎÄ¼ş²ãÃæ):
  - É¾³ı esp_reset_reason() / ESP.getFreeHeap(): ESP32 ×¨Êô API
  - ĞÂÔö InternalFS.begin(): Adafruit nRF52 µÄ LittleFS ³Ö¾Ã»¯³õÊ¼»¯
    (Ìæ´ú ESP32 µÄ NVS/Preferences)
  - LED ¼«ĞÔ: Nice!Nano °åÔØ LED ÊÇµÍµçÆ½µãÁÁ (ESP32 ¶àÎª¸ßµçÆ½)
  - STEP_PIN: Ä¬ÈÏÓÃ P0.17 (Nice!Nano Òı½ÅºÅ 17, D2 ÅÅÕë)

BLE Õ»: NimBLE-Arduino (Ìæ´ú Adafruit Bluefruit, ¸üµÍ¹¦ºÄ)

Ó²¼şÒªÇó:
  - Nice!Nano v2 (nRF52840, Pro Micro ¼æÈİÒı½Å)
  - Arduino IDE + Adafruit nRF52 °å¿¨°ü (Board Manager °²×°)
  - NimBLE-Arduino ¿â (Library Manager °²×°)
  - Ñ¡°å: "PCA10056 nRF52840 DK" (Òı½Å 1:1 Ó³Éä, P0.x = x, P1.x = 32+x)
  - ²»ÒªÑ¡ "Feather nRF52840 Express" ¡ª Feather ±äÌåµÄ g_ADigitalPinMap ²»ÊÇ 1:1,
    »áµ¼ÖÂ STEP_PIN=17 Êµ¼Ê²Ù×÷ P0.28 ¶ø·Ç P0.17, Èı¼«¹Ü¼ÌµçÆ÷µçÂ·ÎŞ·¨Çı¶¯¡£

Ê¹ÓÃ·½·¨:
  1. Arduino IDE -> ¹¤¾ß -> ¿ª·¢°å -> Adafruit nRF52 -> Ñ¡¶ÔÓ¦°å
  2. ¹¤¾ß -> SoftDevice -> S140
  3. ¹¤¾ß -> ´®¿Ú -> Ñ¡¶ÔÓ¦ COM ¿Ú
  4. Ë«»÷ RST ½øÈë UF2 bootloader ºóÉÏ´«, »òÖ±½ÓÉÏ´«
  5. ´®¿Ú¼àÊÓÆ÷ (115200 baud)

¶ÔÓ¦ ESP32: ESP32.ino
================================================================================
*/

// n-able core Ã»ÓĞ Adafruit µÄ OUTPUT_H0H1 Çı¶¯Ç¿¶Èºê£¬Ó³ÉäÎªÆÕÍ¨ OUTPUT¡£
#ifndef OUTPUT_H0H1
#define OUTPUT_H0H1 OUTPUT
#endif

#include "pump_simulator.h"

// Ê¹ÓÃ M640GKit ÃüÃû¿Õ¼ä
using namespace M640GKit;

// Adafruit nRF52 ÓÃ -fno-exceptions ±àÒë, ±ê×¼¿â <vector> ÒıÓÃÁË
// std::__throw_length_error µ«Ã»ÓĞÊµÏÖ, Á´½ÓÊ±±¨ undefined reference¡£
// n-able-Arduino core Á´½ÓÍêÕûµÄ libstdc++_nano, ÒÑÓĞ¸Ã·ûºÅ¡£
// Ê¹ÓÃ weak ÊôĞÔ: ÈôÁ´½ÓÆ÷ÄÜÕÒµ½Ç¿¶¨ÒåÔòÓÃËü, ·ñÔò»ØÍËµ½Õâ¸öÊµÏÖ¡£
namespace std {
    __attribute__((weak)) void __throw_length_error(const char*) { while (1) delay(1); }
}

// ========== Òı½Å¶¨Òå ==========
// STEP_PIN: ¿ØÖÆ TS5A3166 Ä£Äâ¿ª¹Ø, ¸ßµçÆ½µ¼Í¨ = °´ÏÂ±Ã°´¼ü¡£
// Êµ¼Ê³£Á¿¶¨ÒåÔÚ pump_simulator.h ÄÚ (static constexpr int STEP_PIN = 17),
// Ö¸Ïò Nice!Nano µÄ P0.17 (D2 ÅÅÕëÒı½Å)¡£
// ÈçÄã½ÓÔÚÆäËûÒı½Å, ¸Ä pump_simulator.h µÄ STEP_PIN ¼´¿É¡£
// (Adafruit nRF52 Òı½ÅºÅÓ³Éä: P0.x = x; P1.x = 32 + x)

// LED Òı½Å: Nice!Nano °åÔØÀ¶É« LED ÔÚ P0.15, Adafruit nRF52 ºËĞÄÒÑ¶¨ÒåÎª LED_BUILTIN¡£
// °åÔØ LED ÎªµÍµçÆ½µãÁÁ (active-low), Óë ESP32 Ïà·´¡£
#define LED_BUILTIN 15  // Nice!Nano: LED = P0.15

// È«¾ÖÄ£ÄâÆ÷ÊµÀı
M640GPumpSimulator pumpSimulator;

void setup() {
    // ³õÊ¼»¯´®¿Ú (PCA10056 Ä¬ÈÏ×ß USB CDC »ò UART0, È¡¾öÓÚ°å¿¨°üÅäÖÃ)
    Serial.begin(115200);

    // µÈ´ı USB CDC Ã¶¾ÙÍê³É, ·ñÔò setup ÈÕÖ¾»á¶ªÊ§
    uint32_t usbWaitStart = millis();
    while (!Serial && (millis() - usbWaitStart < 5000)) {
        delay(10);
    }

    Serial.println("nRF52840 starting...");
    Serial.println("Version: 2.0.0-nrf52840-NimBLE");
    Serial.flush();

    // ESP32 °æÓĞ esp_reset_reason(); nRF52840 ¿É¶Á NRF_POWER->RESETREAS¡£
    // ÕâÀï´òÓ¡¸´Î»Ô­Òò¼Ä´æÆ÷ (Î»ÑÚÂë), ±ãÓÚÕï¶Ï¡£
    Serial.print("Reset reason (NRF_POWER->RESETREAS raw): 0x");
    Serial.println(NRF_POWER->RESETREAS, HEX);
    // Çå³ı¸´Î»Ô­Òò±êÖ¾ (nRF52 µÄÌØĞÔ: ¸Ã¼Ä´æÆ÷ĞèÊÖ¶¯Çå, ·ñÔò¿ç¸´Î»±£Áô)
    NRF_POWER->RESETREAS = 1;  // Ğ´ÈÎÒâÖµÇå³ı

    Serial.println("\n========================================");
    Serial.println("  M640GKit ±ÃÄ£ÄâÆ÷ (Nice!Nano nRF52840)");
    Serial.println("========================================");
    Serial.println("ÕıÔÚ³õÊ¼»¯...");
    Serial.flush();

    // ---------- ³Ö¾Ã»¯ÒÑ½ûÓÃ ----------
    // nRF52840 LittleFS ÔÚÔËĞĞÊ±Ğ´ flash »á´¥·¢¶ÏÑÔ±ÀÀ£ (pcache->block == 0xffffffff)¡£
    // preferences_nrf52.h µÄ writeFile/readFile ÒÑ¸ÄÎª¿Õ²Ù×÷, ²»ĞèÒª InternalFS¡£
    // Ìø¹ı InternalFS.begin() ±ÜÃâ¹ÒÔØËğ»µµÄÎÄ¼şÏµÍ³µ¼ÖÂ±ÀÀ£ÖØÆô¡£
    Serial.println("Persistence disabled (LittleFS bypassed)");

    // ---------- ³õÊ¼»¯ LED (Nice!Nano °åÔØ LED ÎªµÍµçÆ½µãÁÁ) ----------
    Serial.print("Initializing LED on pin ");
    Serial.println(LED_BUILTIN);
    // °åÔØ LED µÍµçÆ½µãÁÁ: ³õÊ¼Éè HIGH = ÃğµÆ
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // HIGH = Ãğ (active-low)

    // ---------- ³õÊ¼»¯ STEP_PIN (TS5A3166 ¿ØÖÆ, ¸ßµçÆ½µ¼Í¨) ----------
    // STEP_PIN ¶¨ÒåÔÚ pump_simulator.h (= 17 = P0.17 = D2 on Nice!Nano)¡£
    // D2 ÊÇ Nice!Nano ±ßÔµÅÅÕëÉÏµÄÒı½Å, ·½±ã½ÓÏß¡£
    // ±ØĞëÏÈ pinMode(OUTPUT) ÔÙ digitalWrite(LOW),
    // È·±£ pinMode ÇĞ»»Ë²¼äÊä³ö¼Ä´æÆ÷Ä¬ÈÏ LOW, ¿ª¹Ø²»»áÎó´¥·¢¡£
    // OUTPUT_H0H1 = ¸ßÇı¶¯Ç¿¶È (~5mA), ×ãÒÔÇı¶¯Èı¼«¹Ü»ù¼«±¥ºÍµ¼Í¨¡£
    // Ä¬ÈÏ OUTPUT (S0S1 ~0.5mA) Ö»ÄÜÇı¶¯ TS5A3166 CMOS ÊäÈë, ÎŞ·¨Çı¶¯Èı¼«¹Ü+¼ÌµçÆ÷¡£
    pinMode(STEP_PIN, OUTPUT_H0H1);
    digitalWrite(STEP_PIN, LOW);  // TS5A3166 / Èı¼«¹Ü: LOW = ¶Ï¿ª
    Serial.print("STEP_PIN (P0.17=");
    Serial.print(STEP_PIN);
    Serial.println(") initialized as OUTPUT_H0H1 (high-drive), active-high, default LOW)");

    // ÉèÖÃÈ«¾ÖÊµÀıÖ¸Õë (ÓÃÓÚ»Øµ÷)
    gSimulator = &pumpSimulator;

    // ³õÊ¼»¯Ä£ÄâÆ÷ (ÄÚ²¿»áµ÷ gattServer.start() Æô¶¯ BLE)
    Serial.println("Starting pump simulator setup...");
    pumpSimulator.setup();

    Serial.println("³õÊ¼»¯Íê³É!");
    Serial.println("LED will blink fast when advertising, slow when connected");

    // ´òÓ¡ÄÚ´æĞÅÏ¢ (Ìæ´ú ESP32 µÄ ESP.getFreeHeap)
    // Adafruit nRF52 µÄ FreeRTOS Î´µ¼³ö xPortGetFreeHeapSize, ´Ë´¦½ö´òÓ¡ÌáÊ¾
    Serial.println("Free heap: see FreeRTOS stats (xPortGetFreeHeapSize unavailable on nRF52)");

    Serial.println("========================================\n");
    Serial.flush();
}

void loop() {
    // ÔËĞĞÄ£ÄâÆ÷Ö÷Ñ­»·
    pumpSimulator.loop();
    delay(5); // <--- ã€å…³é”®ã€‘ç»™ç³»ç»Ÿåº•å±‚è°ƒåº¦å™¨è®©å‡º 5ms

    // LED Ö¸Ê¾: Î´Á¬½ÓÊ±¿ìÉÁ, ÒÑÁ¬½ÓÊ±³£Ãğ
    static uint32_t lastLedToggle = 0;
    static bool ledOn = false;
    uint32_t now = millis();

    if (pumpSimulator.getIsConnected()) {
        // ÒÑÁ¬½Ó: LED ³£Ãğ
        if (ledOn) {
            ledOn = false;
            digitalWrite(LED_BUILTIN, HIGH);  // HIGH = Ãğ
        }
    } else {
        // Î´Á¬½Ó: ¿ìÉÁ (ÁÁ200ms, Ãğ200ms)
        uint32_t interval = ledOn ? 200 : 200;
        if (now - lastLedToggle >= interval) {
            lastLedToggle = now;
            ledOn = !ledOn;
            digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);  // LOW=ÁÁ, HIGH=Ãğ
        }
    }

    // ÈÃ CPU ¶ÌÔİ sleep, ±ÜÃâ loop() ¿Õ×ªºÄµç¡£
    // 10ms ×ã¹»¶Ì, ²»Ó°Ïì 200ms µÄ simulator update ºÍ LED ÉÁË¸¾«¶È¡£
    delay(10);
}
