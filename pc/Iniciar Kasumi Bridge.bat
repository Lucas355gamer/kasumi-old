@echo off
chcp 65001 >nul
title Kasumi Bridge
cd /d "%~dp0"

where python >nul 2>nul
if errorlevel 1 (
    echo Python nao encontrado. Instale em https://www.python.org/downloads/
    echo Marque "Add python.exe to PATH" durante a instalacao.
    pause
    exit /b 1
)

if not exist ".deps_ok" (
    echo Instalando dependencias, so na primeira vez...
    python -m pip install -r requirements.txt
    if errorlevel 1 (
        echo Falha ao instalar dependencias.
        pause
        exit /b 1
    )
    echo ok> .deps_ok
)

python kasumi_bridge.py %*
pause
