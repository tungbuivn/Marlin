@echo off
rem Wrapper cho menu Send-to: cmd thay %~1 bang duong dan file duoc chon.
rem (Shortcut .lnk KHONG thay %1, nen phai di qua file .cmd nay.)
rem GUI THANG len web server cua man hinh CYD (TBT-CYDLCD): man hinh ghi file vao the SD o khe
rem man hinh roi tu gui M22/M21 cho Marlin. Duong POST thang len module ESP3D (-Target esp3d)
rem va duong upload qua Bluetooth da bi xoa 2026-10-14, nen nay chi con duong nay.
powershell -NoProfile -ExecutionPolicy Bypass -NoExit -File "D:\Marlin\cydlcd\tools\cydlcd-upload.ps1" -File "%~1" -Token "TBT-3D-up"
