# Device Enclosure & Mechanical Build

## Overview

The device enclosure is composed of four main 3D-printed parts, designed to house the airflow system, sensors, and electronics in a compact and functional assembly.

## 3D Printed Parts

- `Snork.stl` — airflow inlet / mask interface  
- `Capot.stl` — main top cover  
- `Enclosure_CO2.stl` — housing for CO₂ sensor  
- `TTGO_T_Display_Front_V2.stl` — front panel for TTGO display  

---

## Design Origin

The main airflow body is based on the excellent work by **Gristle King**:  
https://github.com/meteoscientific/VO2max/wiki

### Modifications

The original design has been adapted to:
- integrate a **Sensirion SDP8xx differential pressure sensor**

---

## Electronics Enclosure

The electronics housing has been fully redesigned.

It is inspired by the TTGO display enclosure by **uzer66**:  
https://www.printables.com/model/119144-lilygo-ttgo-t-displayenclosure

---

## 3D Printing Notes

- Material: **PLA**
- Supports: **standard supports required**

### Assembly details

- Threaded inserts are **partially trimmed** to fit the available depth  
- `Enclosure_CO2.stl` is fixed using **double-sided tape**
- The original connector on the O2 sensor board have been remove to make the connection more compact

---

## Sensor Protection

To protect the gas sensors from humidity:

- A **single-use surgical mask** (cut to size) is placed:
  - in front of the CO₂ sensor  
  - in front of the O₂ sensor  

This acts as a simple **moisture and particle filter**

### Important lesson

An early version without protection resulted in:
- **O₂ sensor failure due to condensation and mucus exposure**

<img width="328" height="340" alt="WhatsApp Image 2026-04-23 at 15 52 05" src="https://github.com/user-attachments/assets/789cc072-efd7-4495-9eb1-6af1356509d6" />

 Proper filtering is **strongly recommended**

---

## Assembly Preview

<img width="650" height="744" alt="WhatsApp Image 2026-04-08 at 15 16 31" src="https://github.com/user-attachments/assets/21562305-cd32-4f0a-9d6b-119ff516613f" />
<img width="1002" height="786" alt="WhatsApp Image 2026-04-07 at 15 55 25" src="https://github.com/user-attachments/assets/c74c30ef-65ad-4344-8c3b-32b321b31ebc" />
<img width="795" height="784" alt="fullsetup" src="https://github.com/user-attachments/assets/0cb6f345-f12f-4b2e-8f51-2fa018ec02e3" />
<img width="1124" height="1326" alt="WhatsApp Image 2026-04-23 at 15 51 55" src="https://github.com/user-attachments/assets/5811434e-7def-4cd3-b50a-7737972d84ea" />



---

