# Kasumi Bridge - instalador de um clique para Windows.
# Instala o Python e o driver ViGEmBus (pelo winget), baixa a ponte da release
# mais recente, instala as bibliotecas e abre o Kasumi Bridge.
#
# Uso (Win+R):
#   powershell -ExecutionPolicy Bypass -NoExit -c "irm https://raw.githubusercontent.com/Lucas355gamer/kasumi-old/main/pc/instalar.ps1 | iex"

$ProgressPreference = 'SilentlyContinue'
$repo = 'Lucas355gamer/kasumi-old'
$dest = Join-Path $env:USERPROFILE 'Downloads\KasumiBridge'

function Passo($t) { Write-Host ''; Write-Host "==> $t" -ForegroundColor Magenta }

function Atualiza-Path {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
                [Environment]::GetEnvironmentVariable('Path', 'User')
}

# Devolve o comando de um Python de verdade (ignora o atalho da Microsoft Store)
function Acha-Python {
    $cands = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313\python.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Python\Python311\python.exe')
    )
    foreach ($c in $cands) { if (Test-Path $c) { return $c } }
    foreach ($n in 'py', 'python') {
        $cmd = Get-Command $n -ErrorAction SilentlyContinue
        if ($cmd -and $cmd.Source -notlike '*WindowsApps*') {
            try {
                $v = & $cmd.Source --version 2>&1
                if ("$v" -match 'Python 3') { return $cmd.Source }
            } catch {}
        }
    }
    return $null
}

Write-Host '============================================' -ForegroundColor Magenta
Write-Host '   Kasumi Bridge - instalacao automatica'     -ForegroundColor Magenta
Write-Host '============================================' -ForegroundColor Magenta

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Host 'O winget nao esta disponivel. Atualize o "Instalador de Aplicativo" pela Microsoft Store.' -ForegroundColor Red
    return
}

# 1) Python
Passo 'Python'
$py = Acha-Python
if ($py) {
    Write-Host "Ja instalado: $py"
} else {
    winget install -e --id Python.Python.3.12 --scope user --silent --accept-package-agreements --accept-source-agreements
    Atualiza-Path
    $py = Acha-Python
    if (-not $py) { Write-Host 'Nao consegui instalar o Python.' -ForegroundColor Red; return }
    Write-Host "Instalado: $py"
}

# 2) Driver do controle virtual
Passo 'Driver ViGEmBus (controle de Xbox virtual)'
if (Get-Service -Name ViGEmBus -ErrorAction SilentlyContinue) {
    Write-Host 'Ja instalado.'
} else {
    Write-Host 'O Windows vai pedir permissao (clique em SIM).' -ForegroundColor Yellow
    winget install -e --id ViGEm.ViGEmBus --accept-package-agreements --accept-source-agreements
    if (Get-Service -Name ViGEmBus -ErrorAction SilentlyContinue) {
        Write-Host 'Instalado. Se o controle nao funcionar, reinicie o PC uma vez.'
    } else {
        Write-Host 'Driver nao instalado; a ponte vai usar teclado e mouse no lugar do controle.' -ForegroundColor Yellow
    }
}

# 3) Ponte
Passo 'Baixando o Kasumi Bridge'
New-Item -ItemType Directory -Force -Path $dest | Out-Null
$zip = Join-Path $env:TEMP 'kasumi-bridge-pc.zip'
Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/$repo/releases/latest/download/kasumi-bridge-pc.zip" -OutFile $zip
Expand-Archive -Force -Path $zip -DestinationPath $dest
Write-Host "Pasta: $dest"

# 4) Bibliotecas
Passo 'Instalando as bibliotecas (so na primeira vez demora)'
& $py -m pip install --disable-pip-version-check -q --upgrade pip
& $py -m pip install --disable-pip-version-check -r (Join-Path $dest 'requirements.txt')
if ($LASTEXITCODE -ne 0) { Write-Host 'Falha ao instalar as bibliotecas.' -ForegroundColor Red; return }
Set-Content -Path (Join-Path $dest '.deps_ok') -Value 'ok'

# 5) Atalho na area de trabalho
try {
    $ws = New-Object -ComObject WScript.Shell
    $lnk = $ws.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'Kasumi Bridge.lnk'))
    $lnk.TargetPath = Join-Path $dest 'Iniciar Kasumi Bridge.bat'
    $lnk.WorkingDirectory = $dest
    $lnk.Save()
    Write-Host 'Atalho "Kasumi Bridge" criado na area de trabalho.'
} catch {}

# 6) Abre a ponte
Passo 'Abrindo o Kasumi Bridge'
Set-Location $dest
& $py (Join-Path $dest 'kasumi_bridge.py')
