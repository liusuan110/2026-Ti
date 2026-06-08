from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


OUT_DIR = Path("outputs/lesson_27")
ITERATIONS = 20
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS


def wrap_pi(angle):
    return (angle + np.pi) % (2 * np.pi) - np.pi


def cordic_atan2(y_in, x_in, iterations=ITERATIONS):
    # Vectoring-mode CORDIC.  Pre-rotate into the right half-plane so the
    # simple iteration covers atan2 over all quadrants.
    x = float(x_in)
    y = float(y_in)
    z = 0.0

    if x < 0:
        if y >= 0:
            x = -x
            y = -y
            z = np.pi
        else:
            x = -x
            y = -y
            z = -np.pi

    for i in range(iterations):
        angle = np.arctan(2.0 ** -i)
        if y > 0:
            x_new = x + y * (2.0 ** -i)
            y_new = y - x * (2.0 ** -i)
            z_new = z + angle
        else:
            x_new = x - y * (2.0 ** -i)
            y_new = y + x * (2.0 ** -i)
            z_new = z - angle
        x, y, z = x_new, y_new, z_new

    return wrap_pi(z)


def fixed_angle_table():
    rows = []
    for i in range(ITERATIONS):
        angle = np.arctan(2.0 ** -i)
        rows.append({
            "i": i,
            "atan_rad": angle,
            "atan_q4_20": int(round(angle * ANGLE_SCALE)),
        })
    return rows


def angle_sweep():
    rows = []
    radius = 80_000
    for deg in range(-180, 181, 5):
        rad = np.deg2rad(deg)
        x = int(round(radius * np.cos(rad)))
        y = int(round(radius * np.sin(rad)))
        exact = np.arctan2(y, x)
        cordic = cordic_atan2(y, x)
        err = wrap_pi(cordic - exact)
        rows.append({
            "angle_deg": deg,
            "i": x,
            "q": y,
            "exact_rad": exact,
            "cordic_rad": cordic,
            "error_rad": err,
            "error_deg": np.rad2deg(err),
            "cordic_q4_20": int(round(cordic * ANGLE_SCALE)),
        })
    return rows


def write_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def write_mem(path, rows):
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            value = row["atan_q4_20"]
            f.write(f"{value:06X}\n")


def plot_rows(rows):
    angle = np.array([row["angle_deg"] for row in rows])
    exact = np.array([row["exact_rad"] for row in rows])
    cordic = np.array([row["cordic_rad"] for row in rows])
    err_deg = np.array([row["error_deg"] for row in rows])

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    axes[0].plot(angle, exact, label="np.atan2")
    axes[0].plot(angle, cordic, linestyle="--", label="CORDIC estimate")
    axes[0].set_title("atan2(Q, I): exact vs CORDIC")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper left")

    axes[1].plot(angle, err_deg, marker="o", linewidth=1.0)
    axes[1].axhline(0.0, color="black", linewidth=0.8)
    axes[1].set_title("CORDIC phase error")
    axes[1].set_xlabel("true angle (deg)")
    axes[1].set_ylabel("error (deg)")
    axes[1].grid(True, alpha=0.3)

    fig.tight_layout()
    png_path = OUT_DIR / "cordic_atan2_error.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    table_rows = fixed_angle_table()
    sweep_rows = angle_sweep()

    table_csv = OUT_DIR / "cordic_atan_table.csv"
    sweep_csv = OUT_DIR / "cordic_atan2_sweep.csv"
    table_mem = OUT_DIR / "cordic_atan_q4_20.mem"
    write_csv(table_csv, table_rows)
    write_csv(sweep_csv, sweep_rows)
    write_mem(table_mem, table_rows)
    png_path = plot_rows(sweep_rows)

    max_abs_err_deg = max(abs(row["error_deg"]) for row in sweep_rows)
    rms_err_deg = np.sqrt(np.mean([row["error_deg"] ** 2 for row in sweep_rows]))

    print("CORDIC atan2 demo")
    print(f"iterations = {ITERATIONS}")
    print(f"angle format = Q4.{ANGLE_FRAC_BITS}")
    print(f"max_abs_error_deg = {max_abs_err_deg:.6f}")
    print(f"rms_error_deg = {rms_err_deg:.6f}")
    print()
    print("First 8 atan table values:")
    print("i,atan_rad,atan_q4_20")
    for row in table_rows[:8]:
        print(f"{row['i']},{row['atan_rad']:.9f},{row['atan_q4_20']}")
    print(f"CSV written to: {table_csv.resolve()}")
    print(f"CSV written to: {sweep_csv.resolve()}")
    print(f"MEM written to: {table_mem.resolve()}")
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
