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
    throw "Vivado was not found. Edit scripts\run_receiver_fsk_rate_h_xsim.ps1 and set the vivado.bat path."
}

$simCsv = Join-Path $root "outputs\receiver_fsk_rate_h\receiver_fsk_rate_h_sim.csv"
if (Test-Path $simCsv) {
    Remove-Item -LiteralPath $simCsv
}

python (Join-Path $root "scripts\prepare_receiver_fsk_rate_h_vectors.py")
& $vivado -mode batch -source (Join-Path $root "scripts\vivado_receiver_fsk_rate_h_pipeline.tcl")

if (-not (Test-Path $simCsv)) {
    throw "Vivado/XSim did not create outputs\receiver_fsk_rate_h\receiver_fsk_rate_h_sim.csv"
}

$failed = Import-Csv $simCsv | Where-Object { $_.ok -ne "1" }
if ($failed) {
    $failed | Select-Object -First 10 | Format-Table -AutoSize
    throw "receiver_fsk_rate_h_pipeline simulation mismatch"
}

Write-Host "PASS: receiver_fsk_rate_h_pipeline XSim"
