#pragma once
#include <stdint.h>
// ESP32-S3-WROOM-1 N16R8 lábkiosztás (lásd a rendszerterv 4. fejezete).
// Kerülendő lábak ezen a modulon: 26-37 (flash és octal PSRAM), 19/20 (USB), 43/44 (UART0),
// strapping: 0, 3, 45, 46. A GPIO48 egyes devkiteken az RGB LED; a TFT_MISO-hoz csak helyfoglalóként van rendelve.
namespace pins {
constexpr uint8_t K_DRV[3]    = {4, 5, 6};      // kontaktor tekercs MOSFET kapu (alap: külső lehúzás)
constexpr uint8_t ARM_EN      = 7;              // ARM-kapcsoló engedélyezés (külső lehúzással)
constexpr uint8_t K_FB[3]     = {15, 16, 17};   // visszajelzés (aktív alacsony, belső + külső felhúzás)
constexpr uint8_t PRESENCE[3] = {8, 9, 18};     // hálózat jelen (AC optocsatoló, 100 Hz impulzus)
constexpr uint8_t AUTO_SENSE  = 21;             // S1 AUTO állásban (aktív alacsony)
constexpr uint8_t ONEWIRE     = 38;             // DS18B20
constexpr uint8_t PZEM_RX     = 39;             // UART2 RX (a PZEM TX felől, 1k/2k osztón át)
constexpr uint8_t PZEM_TX     = 40;
constexpr uint8_t WDT_KICK    = 1;              // külső watchdog billegtetés
constexpr uint8_t LED_OK      = 2;
constexpr uint8_t LED_ERR     = 47;
// 1,44" ST7735 TFT (128x128, SPI, 3,3 V), hardveres SPI (FSPI, a GPIO-mátrixon át). A hátlap (BL) 3V3-ra kötve.
// Bekötés: CS = GPIO10, DC/RS = GPIO12, RST = GPIO14, SCK/CLK = GPIO13, MOSI/SDI = GPIO11.
constexpr uint8_t TFT_MISO      = 48;   // csak helyfoglaló: az ST7735 nem küld adatot vissza
constexpr uint8_t TFT_CS      = 10;
constexpr uint8_t TFT_MOSI    = 11;
constexpr uint8_t TFT_SCK     = 13;
constexpr uint8_t TFT_RST     = 14;
constexpr uint8_t TFT_DC      = 12;
// Lapozó/nyugtázó gomb: GPIO0 = a devkit BOOT gombja (vagy külső gomb GND-re, párhuzamosan).
constexpr uint8_t BTN         = 0;    // lapozás (rövid), hiba nyugtázása (1,5 s)
constexpr uint8_t BTN_DOWN    = 41;   // célhőmérséklet / hiszterézis csökkentése, görgetés (GND-re kötött gomb, belső felhúzás)
constexpr uint8_t BTN_UP      = 42;   // növelés, görgetés
// Szabad: nincs tartalék láb a kijelzőre/gombokra osztva (a háttérvilágítás-vezérlés kimarad)
}
