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
    throw "Vivado was not found. Edit scripts\run_lesson_33_vivado_xsim.ps1 and set the vivado.bat path."
}

$simCsv = Join-Path $root "outputs\lesson_33\ddc_phase_unwrap_core_sim.csv"
if (Test-Path $simCsv) {
    Remove-Item -LiteralPath $simCsv
}

python (Join-Path $root "scripts\lesson_33_prepare_and_check_ddc_phase_unwrap_core.py")
& $vivado -mode batch -source (Join-Path $root "scripts\vivado_lesson_33_ddc_phase_unwrap_core.tcl")
if (-not (Test-Path $simCsv)) {
    throw "Vivado/XSim did not create outputs\lesson_33\ddc_phase_unwrap_core_sim.csv"
}
python (Join-Path $root "scripts\lesson_33_prepare_and_check_ddc_phase_unwrap_core.py")
