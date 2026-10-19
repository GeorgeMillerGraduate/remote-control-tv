@echo off
cd /d "%~dp0"
call mvn clean verify
if errorlevel 1 exit /b 1
java -jar target\remote-server-0.1.0.jar
