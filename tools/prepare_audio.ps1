param(
    [string]$InputDir = ".",
    [string]$OutputDir = "assets/audio"
)

$ErrorActionPreference = "Stop"

$python = "C:\Users\User\AppData\Local\Programs\Python\Python313\python.exe"
$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
if (-not $ffmpeg) {
    $ffmpegPath = & $python -c "import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())"
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $ffmpegPath)) {
        throw "ffmpeg not found. Install ffmpeg or Python package imageio_ffmpeg, then rerun this script."
    }
} else {
    $ffmpegPath = $ffmpeg.Source
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$tracks = @(
    @{ In = "高橋洋子 - (1786262382)残酷な天使のテーゼ.mp3"; Out = "track0.adpcm" },
    @{ In = "宇多田光 - One Last Kiss.mp3"; Out = "track1.adpcm" },
    @{ In = "宇多田ヒカル - Beautiful World.mp3"; Out = "track2.adpcm" }
)

foreach ($track in $tracks) {
    $inputPath = Join-Path $InputDir $track.In
    if (-not (Test-Path -LiteralPath $inputPath)) {
        throw "Missing input file: $inputPath"
    }

    $wavPath = Join-Path $OutputDir ($track.Out + ".wav")
    $outPath = Join-Path $OutputDir $track.Out
    & $ffmpegPath -y -i $inputPath -ac 1 -ar 8000 -sample_fmt s16 $wavPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & $python "tools/encode_adpcm.py" $wavPath $outPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Remove-Item -LiteralPath $wavPath -Force
}
