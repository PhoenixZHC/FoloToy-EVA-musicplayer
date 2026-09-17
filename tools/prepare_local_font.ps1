param(
    [Parameter(Mandatory = $true)]
    [string]$FontFile
)

$root = Split-Path -Parent $PSScriptRoot
$font = (Resolve-Path -LiteralPath $FontFile -ErrorAction Stop).Path
$pythonExe = Join-Path $root '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $pythonExe)) { $pythonExe = 'py' }
$previousFont = [Environment]::GetEnvironmentVariable('EVA_FONT_PATH', 'Process')

try {
    Push-Location -LiteralPath $root
    $env:EVA_FONT_PATH = $font

    & $pythonExe -c 'import PIL, fontTools'
    if ($LASTEXITCODE -ne 0) { throw 'Install Pillow and fonttools in the selected Python environment.' }

    & $pythonExe tools/prepare_text_assets.py
    if ($LASTEXITCODE -ne 0) { throw 'Text asset generation failed.' }
    & $pythonExe tools/prepare_text_buttons.py
    if ($LASTEXITCODE -ne 0) { throw 'Button asset generation failed.' }

    foreach ($size in @(14, 20)) {
        $name = 'eva_font_matisse_' + $size
        & npx.cmd --yes --package lv_font_conv@1.5.3 lv_font_conv --font $font --range '0x20-0x7e' --size $size --bpp 4 --no-compress --format lvgl --lv-include lvgl.h --lv-font-name $name -o ('main/' + $name + '.c')
        if ($LASTEXITCODE -ne 0) { throw "LVGL font generation failed for size $size." }
    }

    & $pythonExe tools/gen_web_ui.py
    if ($LASTEXITCODE -ne 0) { throw 'Web page generation failed.' }
    Write-Output 'Local font assets are ready. They are excluded from Git.'
} finally {
    [Environment]::SetEnvironmentVariable('EVA_FONT_PATH', $previousFont, 'Process')
    Pop-Location
}
