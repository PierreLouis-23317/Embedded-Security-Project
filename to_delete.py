import serial, time

PORT = "COM9"
BAUDRATE = 9600

with serial.Serial(PORT, BAUDRATE, timeout=2) as ser:
    time.sleep(0.5)
    ser.reset_input_buffer()

    t0 = time.perf_counter()
    ser.write(b"aaaaaaaa\n")
    ser.flush()
    resp = ser.read_until(b"\n")
    t1 = time.perf_counter()

    print("Réponse:", resp)
    print(f"Temps total: {(t1-t0)*1000:.1f} ms")