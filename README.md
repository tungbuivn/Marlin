Thí project intend to create custom pcb for the voron 2.4 which capacitive of:
- milling-able by mini cnc machine
- use board STM32F407VET6
- ability handle 740W bed power
- Support two power source at once
- Support atleast 7 motor
- Support external arduino sdcard (if there is free pin, need to separate from board to put it any where)
- z-axis divide into front and back
- use tmc 2009 driver with USART

## Schematic tests
[x] LCD Display 2.8"
[ ] Rotary Encoder
[x] SDCARD Connection
[ ] Fan hot end
[ ] Fan layer
[ ] Fan electric
[ ] Temp bed
[ ] Temp hotend
[ ] EEProm
[ ] TMC2209 UART
[ ] Motor rotation (Voron style)
[ ] Sensor LJC 18mm, Lj123A4 Z-bx
[ ] Sensor Servo
[ ] Deploy/Stow Sensor (turn on/ off 24v sensor)

Changes:
31/10: 
- Add support servo, bltouch, 24 sensor
- Add invert input signal Z axis
- Update schematic serial connection for the motor
- Update board pin config