; PlatformIO Project Configuration File
;
;   Build options: build flags, source filter
;   Upload options: custom upload port, speed and extra flags
;   Library options: dependencies, extra library storages
;   Advanced options: extra scripting
;
; Please visit documentation for the other options and examples
; https://docs.platformio.org/page/projectconf.html

[platformio]
; esp32dev works for the majority of ESP32 based dev boards, change this 
; for your board if desired
default_envs = esp32dev
src_dir = .

[common]
framework = arduino
monitor_speed = 115200
lib_deps =
    ArduinoJson
    Wire
    HTTPClient
    WiFiClientSecure
    Adafruit BusIO

[MiniGrafx]
lib_deps = 
    Mini Grafx
    FS
    SPIFFS

[GxEPD2]
lib_deps = 
    GxEPD2
    U8g2_for_Adafruit_GFX


[base:esp32]
lib_extra_dirs = ../..
platform = espressif32
framework = ${common.framework}
lib_deps = ${common.lib_deps} ${gfxlib.lib_deps}

[env:esp32dev]
board = esp32dev
extends = base:esp32

[env:esp32doit-devkit-v1]
board = esp32doit-devkit-v1
extends = base:esp32

[env:lolin_d32_pro]
board = lolin_d32_pro
extends = base:esp32


