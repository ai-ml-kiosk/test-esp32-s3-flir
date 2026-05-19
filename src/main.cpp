#include <Arduino.h>

#if FLIR_REAL_APP
#include "flir/flir_app.h"
#else
#include "demo/demo_app.h"
#endif

void setup() {
#if FLIR_REAL_APP
  FlirApp::setup();
#else
  DemoApp::setup();
#endif
}

void loop() {
#if FLIR_REAL_APP
  FlirApp::loop();
#else
  DemoApp::loop();
#endif
}
