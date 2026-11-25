@echo off
chcp 65001 >nul
title Kasumi Bridge
cd /d "%~dp0"

rem Procura um Python de verdade (o "python" da Microsoft Store e so um atalho vazio)
set "PY="
py -3 --version >nul 2>nul && set "PY=py -3"
if not defined PY python --version >nul 2>nul && set "PY=python"

if not defined PY (
    echo.
    echo  O Python nao esta instalado neste PC.
    echo.
    echo  Posso instalar agora pelo winget ^(o instalador oficial do Windows^).
    choice /c SN /m " Instalar o Python 3.12 agora"
    if errorlevel 2 (
        echo.
        echo  Instale manualmente em https://www.python.org/downloads/
        echo  e marque "Add python.exe to PATH". Depois abra este arquivo de novo.
        pause
        exit /b 1
    )
    winget install -e --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements
    echo.
    echo  Pronto. FECHE esta janela e abra o "Iniciar Kasumi Bridge" de novo.
    pause
    exit /b 0
)

if not exist ".deps_ok" (
    echo Instalando dependencias, so na primeira vez...
    %PY% -m pip install --upgrade pip
    %PY% -m pip install -r requirements.txt
    if errorlevel 1 (
        echo Falha ao instalar dependencias.
        pause
        exit /b 1
    )
    echo ok> .deps_ok
)

%PY% kasumi_bridge.py %*
pause
