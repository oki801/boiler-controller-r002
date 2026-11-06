#include <Arduino.h>
#include <Supervisor.h>

// A Supervisor példányt a setup()-ban hozzuk létre, így a hardverhez nyúló konstruktorok
// az Arduino-környezet inicializálása után futnak.
static Supervisor* sup = nullptr;

void setup() {
  sup = new Supervisor();
  sup->begin();
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));   // minden munkát a FreeRTOS task-ok végeznek
}
