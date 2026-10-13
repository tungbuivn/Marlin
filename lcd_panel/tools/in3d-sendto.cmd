@echo off
rem Wrapper cho menu Send-to: cmd thay %~1 bang duong dan file duoc chon.
rem (Shortcut .lnk KHONG thay %1, nen phai di qua file .cmd nay.)
rem MAC DINH: gui len PANEL qua WiFi (panel tu day sang ESP3D + tu gui M22/M21 cho Marlin).
rem Duong Bluetooth cu:  doi panel-upload.ps1 thanh bt-upload-socket.ps1 -File "%~1" -Token "TBT-3D-up"
rem Duong gui thang ESP3D: them  -Target esp3d -InitSd -MarlinPort COM4
powershell -NoProfile -ExecutionPolicy Bypass -NoExit -File "D:\Marlin\lcd_panel\tools\panel-upload.ps1" -File "%~1" -Token "TBT-3D-up"
