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
    throw "Vivado was not found. Edit scripts\run_lesson_34_vivado_xsim.ps1 and set the vivado.bat path."
}

$simCsv = Join-Path $root "outputs\lesson_34\phase_slope_estimator_sim.csv"
if (Test-Path $simCsv) {
    Remove-Item -LiteralPath $simCsv
}

python (Join-Path $root "scripts\lesson_34_prepare_and_check_phase_slope_estimator.py")
& $vivado -mode batch -source (Join-Path $root "scripts\vivado_lesson_34_phase_slope_estimator.tcl")
if (-not (Test-Path $simCsv)) {
    throw "Vivado/XSim did not create outputs\lesson_34\phase_slope_estimator_sim.csv"
}
python (Join-Path $root "scripts\lesson_34_prepare_and_check_phase_slope_estimator.py")
