$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$vivadoCandidates = @(
    "D:\Xilinx\Vivado\2023.1\bin\vivado.bat",
    "C:\Xilinx\Vivado\2023.1\bin\vivado.bat"
)

$vivado = $null
foreach ($candidate in $vivadoCandidates) {
    if (Test-Path -LiteralPath $candidate) {
        $vivado = $candidate
        break
    }
}

if (-not $vivado) {
    $cmd = Get-Command vivado -ErrorAction SilentlyContinue
    if ($cmd) {
        $vivado = $cmd.Source
    }
}

if (-not $vivado) {
    throw "Vivado was not found. Edit scripts\run_iq_feature_xsim.ps1 and set the vivado.bat path."
}

$simCsv = Join-Path $root "outputs\iq_feature\iq_feature_sim.csv"
if (Test-Path $simCsv) {
    Remove-Item -LiteralPath $simCsv
}

python (Join-Path $root "scripts\prepare_iq_feature_vectors.py")
& $vivado -mode batch -source (Join-Path $root "scripts\vivado_iq_feature_extractor.tcl")

if (-not (Test-Path $simCsv)) {
    throw "Vivado/XSim did not create outputs\iq_feature\iq_feature_sim.csv"
}

$failed = Import-Csv $simCsv | Where-Object { $_.ok -ne "1" }
if ($failed) {
    $failed | Select-Object -First 10 | Format-Table -AutoSize
    throw "iq_feature_extractor simulation mismatch"
}

Write-Host "PASS: iq_feature_extractor XSim"
