from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_30"

DATA_BITS = 21
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS
ITERATIONS = 20
RADIUS = 80_000


def signed_to_twos_complement(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return value


def wrap_pi_q(angle_q):
    pi_q = int(round(np.pi * ANGLE_SCALE))
    two_pi_q = 2 * pi_q
    while angle_q > pi_q:
        angle_q -= two_pi_q
    while angle_q < -pi_q:
        angle_q += two_pi_q
    return angle_q


def atan_table_q():
    return [int(round(np.arctan(2.0 ** -i) * ANGLE_SCALE)) for i in range(ITERATIONS)]


def pipeline_cordic_model(i_value, q_value):
    table = atan_table_q()
    pi_q = int(round(np.pi * ANGLE_SCALE))

    x = int(i_value)
    y = int(q_value)
    z = 0

    if x < 0:
        x = -x
        y = -y
        z = pi_q if q_value >= 0 else -pi_q

    for step, angle_q in enumerate(table):
        if y == 0:
            continue

        x_shift = x >> step
        y_shift = y >> step

        if y > 0:
            x, y, z = x + y_shift, y - x_shift, z + angle_q
        else:
            x, y, z = x - y_shift, y + x_shift, z - angle_q

    return wrap_pi_q(z)


def make_vectors():
    rows = []
    for deg in range(-180, 181, 5):
        rad = np.deg2rad(deg)
        i_value = int(round(RADIUS * np.cos(rad)))
        q_value = int(round(RADIUS * np.sin(rad)))
        exact = np.arctan2(q_value, i_value)
        exact_q = int(round(exact * ANGLE_SCALE))
        cordic_q = pipeline_cordic_model(i_value, q_value)
        error_q = wrap_pi_q(cordic_q - exact_q)
        rows.append({
            "n": len(rows),
            "angle_deg": deg,
            "i": i_value,
            "q": q_value,
            "exact_q4_20": exact_q,
            "pipeline_q4_20": cordic_q,
            "error_q4_20": error_q,
            "error_deg": error_q / ANGLE_SCALE * 180.0 / np.pi,
        })

    extras = [
        (0, 0),
        (RADIUS, 0),
        (0, RADIUS),
        (-RADIUS, 0),
        (0, -RADIUS),
        (RADIUS, RADIUS // 2),
        (-RADIUS, RADIUS // 2),
        (-RADIUS, -RADIUS // 2),
    ]
    for i_value, q_value in extras:
        exact = np.arctan2(q_value, i_value)
        exact_q = int(round(exact * ANGLE_SCALE))
        cordic_q = pipeline_cordic_model(i_value, q_value)
        error_q = wrap_pi_q(cordic_q - exact_q)
        rows.append({
            "n": len(rows),
            "angle_deg": np.rad2deg(exact),
            "i": i_value,
            "q": q_value,
            "exact_q4_20": exact_q,
            "pipeline_q4_20": cordic_q,
            "error_q4_20": error_q,
            "error_deg": error_q / ANGLE_SCALE * 180.0 / np.pi,
        })

    return rows


def write_mem(path, rows, key, bits):
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            code = signed_to_twos_complement(int(row[key]), bits)
            f.write(f"{code:0{(bits + 3) // 4}X}\n")


def write_expected_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def read_sim_csv(path):
    rows = []
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({
                "n": int(row["n"]),
                "i": int(row["i"]),
                "q": int(row["q"]),
                "phase_q4_20": int(row["phase_q4_20"]),
            })
    return rows


def compare_sim_with_expected(expected_rows):
    sim_path = OUT_DIR / "cordic_atan2_pipeline_sim.csv"
    if not sim_path.exists():
        print("No Vivado/XSim CSV found yet.")
        print(f"Expected simulation output path: {sim_path}")
        return

    expected_by_n = {row["n"]: row for row in expected_rows}
    sim_rows = read_sim_csv(sim_path)
    mismatches = []
    for actual in sim_rows:
        expected = expected_by_n[actual["n"]]
        if actual["phase_q4_20"] != expected["pipeline_q4_20"]:
            mismatches.append((actual["n"], expected["pipeline_q4_20"], actual["phase_q4_20"]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for n, expected, actual in mismatches[:10]:
            print(f"n={n} expected={expected} actual={actual}")
    else:
        print(f"Vivado/XSim output matches expected pipeline CORDIC output for {len(sim_rows)} vectors.")


def plot_rows(rows):
    angle = np.array([row["angle_deg"] for row in rows])
    exact = np.array([row["exact_q4_20"] for row in rows]) / ANGLE_SCALE
    cordic = np.array([row["pipeline_q4_20"] for row in rows]) / ANGLE_SCALE
    err_deg = np.array([row["error_deg"] for row in rows])

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    axes[0].scatter(angle, exact, s=12, label="exact atan2")
    axes[0].scatter(angle, cordic, s=12, marker="x", label="pipeline CORDIC")
    axes[0].set_title("Pipeline CORDIC atan2 expected output")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper left")

    axes[1].scatter(angle, err_deg, s=12)
    axes[1].axhline(0.0, color="black", linewidth=0.8)
    axes[1].set_title("Expected CORDIC error vs exact atan2")
    axes[1].set_xlabel("input angle (deg)")
    axes[1].set_ylabel("error (deg)")
    axes[1].grid(True, alpha=0.3)

    fig.tight_layout()
    png_path = OUT_DIR / "cordic_atan2_pipeline_expected.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    rows = make_vectors()
    write_mem(SIM_DIR / "pipeline_cordic_i_81.mem", rows, "i", DATA_BITS)
    write_mem(SIM_DIR / "pipeline_cordic_q_81.mem", rows, "q", DATA_BITS)

    expected_csv = OUT_DIR / "cordic_atan2_pipeline_expected.csv"
    write_expected_csv(expected_csv, rows)
    png_path = plot_rows(rows)

    max_abs_err = max(abs(row["error_deg"]) for row in rows)
    rms_err = np.sqrt(np.mean([row["error_deg"] ** 2 for row in rows]))

    print("Lesson 30 pipeline CORDIC assets prepared")
    print(f"vectors = {len(rows)}")
    print(f"iterations = {ITERATIONS}")
    print("throughput = one accepted vector per clock")
    print(f"latency = {ITERATIONS} clocks")
    print(f"angle scale = 2^{ANGLE_FRAC_BITS}")
    print(f"max_abs_error_deg = {max_abs_err:.6f}")
    print(f"rms_error_deg = {rms_err:.6f}")
    print(f"i mem: {(SIM_DIR / 'pipeline_cordic_i_81.mem').resolve()}")
    print(f"q mem: {(SIM_DIR / 'pipeline_cordic_q_81.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print(f"expected plot: {png_path.resolve()}")
    print()
    print("First 10 expected vectors:")
    print("n,angle_deg,i,q,exact_q4_20,pipeline_q4_20,error_deg")
    for row in rows[:10]:
        print(
            f"{row['n']},{row['angle_deg']},{row['i']},{row['q']},"
            f"{row['exact_q4_20']},{row['pipeline_q4_20']},{row['error_deg']:.6f}"
        )
    print()
    compare_sim_with_expected(rows)


if __name__ == "__main__":
    main()
