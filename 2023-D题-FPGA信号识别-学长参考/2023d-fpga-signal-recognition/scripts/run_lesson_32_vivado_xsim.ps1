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
    throw "Vivado was not found. Edit scripts\run_lesson_32_vivado_xsim.ps1 and set the vivado.bat path."
}

python (Join-Path $root "scripts\lesson_32_prepare_and_check_phase_unwrap_detrend.py")
& $vivado -mode batch -source (Join-Path $root "scripts\vivado_lesson_32_phase_unwrap.tcl")
if (-not (Test-Path (Join-Path $root "outputs\lesson_32\phase_unwrap_sim.csv"))) {
    throw "Vivado/XSim did not create outputs\lesson_32\phase_unwrap_sim.csv"
}
python (Join-Path $root "scripts\lesson_32_prepare_and_check_phase_unwrap_detrend.py")
