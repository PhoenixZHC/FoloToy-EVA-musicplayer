param(
    [Parameter(Mandatory = $true)][string]$InputFile
)

$ErrorActionPreference = 'Stop'
$py = Get-Command py -ErrorAction SilentlyContinue
if (-not $py) { throw 'Python launcher py is unavailable' }
& $py.Source -3.13 --version | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Python 3.13 is unavailable' }
if (-not (Test-Path -LiteralPath $InputFile)) { throw "Missing audio: $InputFile" }
$ffmpegCommand = Get-Command ffmpeg -ErrorAction SilentlyContinue
if ($ffmpegCommand) {
    $ffmpeg = $ffmpegCommand.Source
} else {
    $ffmpeg = & $py.Source -3.13 -c 'import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())'
    if ($LASTEXITCODE -ne 0) { throw 'ffmpeg is unavailable' }
}
$wav = 'assets/audio/startup.adpcm.wav'
$out = 'assets/audio/startup.adpcm'
& $ffmpeg -y -i $InputFile -ac 1 -ar 8000 -sample_fmt s16 $wav
if ($LASTEXITCODE -ne 0) { throw 'ffmpeg conversion failed' }
& $py.Source -3.13 'tools/encode_adpcm.py' $wav $out
if ($LASTEXITCODE -ne 0) { throw 'ADPCM encoding failed' }
Write-Output "Generated $out"
