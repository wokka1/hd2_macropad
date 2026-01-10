/*
 * Minimal test for ESP32-8048S070 to verify basic functionality
 * This bypasses all the complex UI code to test just Serial and basic GPIO
 */

#ifdef ESP32_8048S070_TEST

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(2000); // Give serial plenty of time

    Serial.println("\n\n======================");
    Serial.println("MINIMAL TEST");
    Serial.println("ESP32-8048S070");
    Serial.println("======================");

    pinMode(2, OUTPUT); // Backlight pin
    digitalWrite(2, HIGH);
    Serial.println("Backlight pin set HIGH");
}

void loop() {
    static uint32_t counter = 0;
    Serial.print("Loop: ");
    Serial.println(counter++);
    delay(1000);
}

#endif
