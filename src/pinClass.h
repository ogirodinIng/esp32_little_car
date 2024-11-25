#include <Arduino.h>

class PinClass 
{
  private:
    int ref;
    bool light;
    bool digitalLight;
  public:  
    void init(uint8_t nb, int pinModeValue) {
      ref = nb;
      light = false;
      pinMode(ref, pinModeValue);
    };
    void toggleLight() {
      Serial.println("normalement on toggle");
      Serial.println(light);
      light = !light;
      digitalWrite(ref, light);
    };
    void digitalSpeed(int16_t joystickValue) {
      analogWrite(ref, joystickValue);
    }
    void lightOnOff(bool onOff) {
      digitalWrite(ref, onOff);
    };
    bool buttonPressed() {
      return digitalRead(ref);
    };
};