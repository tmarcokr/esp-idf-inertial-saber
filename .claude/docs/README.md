# Reference Documents

Vendor datasheets and manuals for the InertialSaber OS hardware (ESP32-S3, MAX98357A, MPU-6050). The documents are not redistributed in this repository; get them from the vendors through the links below. Local copies may be kept in this folder: `*.pdf` files in `.claude/docs/` are ignored by git.

Links checked on 2026-10-10.

| Document | Vendor | Link |
|:--|:--|:--|
| ESP32-S3 Series Datasheet | Espressif | https://documentation.espressif.com/esp32-s3_datasheet_en.html |
| ESP32-S3 Technical Reference Manual | Espressif | https://documentation.espressif.com/esp32-s3_technical_reference_manual_en.pdf |
| Technical documents (module datasheets, hardware design guidelines, errata, dev-kit guides) | Espressif | https://www.espressif.com/en/support/download/documents |
| MAX98357A I2S class-D amplifier (product page and datasheet) | Analog Devices | https://www.analog.com/en/products/max98357a.html |
| MPU-6050 six-axis IMU | TDK InvenSense | No verified official link (see below) |

**MPU-6050**: the part is no longer listed on the current TDK InvenSense website (https://invensense.tdk.com/en-us), and its former document links redirect to the site search. The documents to look for are "PS-MPU-6000A-00 – MPU-6000 and MPU-6050 Datasheet" and "RM-MPU-6000A-00 – MPU-6000 and MPU-6050 Register Map".

The ESP32-C6 documents and the original ESP32 Technical Reference Manual that were kept here earlier are not listed: ESP32-C6 support was removed, and the ESP32 manual does not cover the ESP32-S3.
