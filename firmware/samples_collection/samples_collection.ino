import serial
import csv
import os
import time
import math
import statistics
from datetime import datetime


# ============================================================
# SETTINGS
# ============================================================

COM_PORT = "COM15"
BAUD_RATE = 115200

TEST_DURATION_MINUTES = 10

CSV_FILE = "temperature_sensor_data.csv"


# ============================================================
# FUNCTIONS
# ============================================================

def calculate_rmse(errors):

    if len(errors) == 0:
        return float("nan")

    return math.sqrt(
        sum(error ** 2 for error in errors) / len(errors)
    )


# ============================================================
# PROGRAM START
# ============================================================

print()
print("==========================================================")
print("       EE2120 TEMPERATURE SENSOR CHARACTERIZATION")
print("==========================================================")
print()


# ============================================================
# ASK ACTUAL / REFERENCE TEMPERATURE FIRST
# ============================================================

while True:

    try:

        reference_temp = float(
            input(
                "Enter the ACTUAL temperature from the "
                "reference thermometer (°C): "
            )
        )

        break

    except ValueError:

        print("Please enter a valid number.")
        print()


# ============================================================
# CREATE SESSION ID
# ============================================================

session_id = datetime.now().strftime("%Y%m%d_%H%M%S")


print()
print("----------------------------------------------------------")
print("Experiment Information")
print("----------------------------------------------------------")

print(f"Session ID          : {session_id}")
print(f"Actual Temperature  : {reference_temp:.2f} °C")
print(f"Duration            : {TEST_DURATION_MINUTES} minutes")
print(f"CSV File            : {CSV_FILE}")

print("----------------------------------------------------------")
print()


# ============================================================
# CONNECT TO ESP32
# ============================================================

try:

    ser = serial.Serial(
        COM_PORT,
        BAUD_RATE,
        timeout=2
    )

except serial.SerialException as error:

    print("ERROR: Could not open serial port.")
    print()
    print("Port:", COM_PORT)
    print("Error:", error)
    print()
    print("Check:")
    print("1. ESP32 COM port")
    print("2. Arduino Serial Monitor is closed")
    print("3. ESP32 is connected")
    print()

    exit()


print("Connecting to ESP32...")

# ESP32 normally resets when serial port is opened
time.sleep(2)

# Remove old buffered serial data
ser.reset_input_buffer()

print("ESP32 connected successfully.")
print()


# ============================================================
# CSV FILE SETUP
# ============================================================

file_exists = os.path.isfile(CSV_FILE)


csv_file = open(
    CSV_FILE,
    "a",
    newline="",
    encoding="utf-8"
)


writer = csv.writer(csv_file)


# Create header only for a new file
if not file_exists:

    writer.writerow([
        "timestamp",
        "session_id",
        "elapsed_seconds",
        "reference_temperature_C",
        "DS18B20_raw_C",
        "NTC_raw_C",
        "DS18B20_error_C",
        "NTC_error_C"
    ])

    csv_file.flush()


# ============================================================
# STORAGE FOR CURRENT EXPERIMENT
# ============================================================

ds_values = []
ntc_values = []

ds_errors = []
ntc_errors = []


# ============================================================
# TEST TIMING
# ============================================================

duration_seconds = TEST_DURATION_MINUTES * 60

start_time = time.time()

sample_number = 0


print("==========================================================")
print("DATA COLLECTION STARTED")
print("==========================================================")
print()

print(
    f"{'Sample':<8}"
    f"{'Time(s)':<12}"
    f"{'Actual':<12}"
    f"{'DS18B20':<12}"
    f"{'NTC':<12}"
    f"{'DS Error':<12}"
    f"{'NTC Error':<12}"
)

print("-" * 80)


# ============================================================
# DATA COLLECTION
# ============================================================

try:

    while (time.time() - start_time) < duration_seconds:

        # Read one serial line
        line = ser.readline().decode(
            "utf-8",
            errors="ignore"
        ).strip()


        # ESP32 must send:
        #
        # DATA,28.94,31.11
        #
        # Ignore all other Serial messages

        if not line.startswith("DATA,"):
            continue


        parts = line.split(",")


        if len(parts) != 3:
            continue


        # ----------------------------------------------------
        # CONVERT SENSOR DATA
        # ----------------------------------------------------

        try:

            ds_temp = float(parts[1])
            ntc_temp = float(parts[2])

        except ValueError:

            continue


        # ----------------------------------------------------
        # INVALID READING CHECK
        # ----------------------------------------------------

        # DS18B20 operating range check
        if ds_temp < -55 or ds_temp > 125:

            print(
                "Invalid DS18B20 reading rejected:",
                ds_temp
            )

            continue


        # Basic NTC temperature range check
        if ntc_temp < -55 or ntc_temp > 150:

            print(
                "Invalid NTC reading rejected:",
                ntc_temp
            )

            continue


        # ----------------------------------------------------
        # TIME
        # ----------------------------------------------------

        elapsed = time.time() - start_time


        timestamp = datetime.now().strftime(
            "%Y-%m-%d %H:%M:%S.%f"
        )[:-3]


        # ----------------------------------------------------
        # ERROR RELATIVE TO REFERENCE
        # ----------------------------------------------------

        ds_error = ds_temp - reference_temp

        ntc_error = ntc_temp - reference_temp


        # ----------------------------------------------------
        # STORE DATA
        # ----------------------------------------------------

        ds_values.append(ds_temp)
        ntc_values.append(ntc_temp)

        ds_errors.append(ds_error)
        ntc_errors.append(ntc_error)


        sample_number += 1


        # ----------------------------------------------------
        # APPEND SAMPLE TO CSV
        # ----------------------------------------------------

        writer.writerow([
            timestamp,
            session_id,
            round(elapsed, 2),
            round(reference_temp, 3),
            round(ds_temp, 3),
            round(ntc_temp, 3),
            round(ds_error, 4),
            round(ntc_error, 4)
        ])


        # Save continuously
        csv_file.flush()


        # ----------------------------------------------------
        # PRINT LIVE DATA
        # ----------------------------------------------------

        print(
            f"{sample_number:<8}"
            f"{elapsed:<12.1f}"
            f"{reference_temp:<12.2f}"
            f"{ds_temp:<12.2f}"
            f"{ntc_temp:<12.2f}"
            f"{ds_error:<+12.2f}"
            f"{ntc_error:<+12.2f}"
        )


except KeyboardInterrupt:

    print()
    print("Experiment stopped manually by user.")


finally:

    ser.close()
    csv_file.close()


# ============================================================
# CALCULATE STATISTICS
# ============================================================

print()
print()
print("==========================================================")
print("EXPERIMENT COMPLETE")
print("==========================================================")

print()
print(f"Session ID           : {session_id}")
print(f"Actual Temperature   : {reference_temp:.2f} °C")
print(f"Samples Collected    : {sample_number}")


# ============================================================
# DS18B20 RESULTS
# ============================================================

if len(ds_values) > 1:

    ds_mean = statistics.mean(ds_values)

    ds_std = statistics.stdev(ds_values)

    ds_bias = statistics.mean(ds_errors)

    ds_rmse = calculate_rmse(ds_errors)

    ds_min = min(ds_values)

    ds_max = max(ds_values)


    print()
    print("----------------------------------------------------------")
    print("DS18B20 RESULTS")
    print("----------------------------------------------------------")

    print(f"Mean Temperature     : {ds_mean:.4f} °C")

    print(
        f"Noise (Std Dev)      : {ds_std:.4f} °C"
    )

    print(
        f"Bias                  : {ds_bias:+.4f} °C"
    )

    print(
        f"RMSE                  : {ds_rmse:.4f} °C"
    )

    print(
        f"Minimum               : {ds_min:.4f} °C"
    )

    print(
        f"Maximum               : {ds_max:.4f} °C"
    )


# ============================================================
# NTC RESULTS
# ============================================================

if len(ntc_values) > 1:

    ntc_mean = statistics.mean(ntc_values)

    ntc_std = statistics.stdev(ntc_values)

    ntc_bias = statistics.mean(ntc_errors)

    ntc_rmse = calculate_rmse(ntc_errors)

    ntc_min = min(ntc_values)

    ntc_max = max(ntc_values)


    print()
    print("----------------------------------------------------------")
    print("NTC RESULTS")
    print("----------------------------------------------------------")

    print(
        f"Mean Temperature     : {ntc_mean:.4f} °C"
    )

    print(
        f"Noise (Std Dev)      : {ntc_std:.4f} °C"
    )

    print(
        f"Bias                  : {ntc_bias:+.4f} °C"
    )

    print(
        f"RMSE                  : {ntc_rmse:.4f} °C"
    )

    print(
        f"Minimum               : {ntc_min:.4f} °C"
    )

    print(
        f"Maximum               : {ntc_max:.4f} °C"
    )


print()
print("==========================================================")
print("CSV DATA SAVED")
print("==========================================================")

print()
print(f"File: {CSV_FILE}")

print()
print(
    "Run this program again for another temperature."
)

print(
    "The new experiment will be APPENDED to the same CSV."
)

print()
