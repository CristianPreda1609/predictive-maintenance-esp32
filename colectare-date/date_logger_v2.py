import serial
import csv
import time
import os
import sys


# --- CONFIGURARE ---
SERIAL_PORT = "COM3"   # Verifica daca e portul corect!
BAUD_RATE = 115200
FILENAME = "fixare_slaba_255.csv"
CURRENT_LABEL = 4
DURATION_SECONDS = 183 # 3 Minute

# --- 1. PREGATIRE FOLDER SI FISIER ---
# Cream folderul 'date' daca nu exista, ca sa nu dea eroare
if not os.path.exists("date"):
    os.makedirs("date")

full_path = os.path.join("date", FILENAME)

# --- 2. CONECTARE SERIALA ---
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f" Conectat la {SERIAL_PORT}")
    print("Astept 2 secunde pentru resetarea ESP32...")
    time.sleep(2)
except serial.SerialException:
    print(f" Eroare: Nu pot deschide portul {SERIAL_PORT}")
    print("  Verifica daca ai inchis Serial Monitor in PlatformIO!")
    sys.exit()

# --- 3. COLECTARE AUTOMATA ---
print(f"\n START COLECTARE: {DURATION_SECONDS} secunde (3 minute)")
print(f" Fisier: {full_path}")
print("Nu misca firele!...\n")

start_time = time.time()
line_count = 0

with open(full_path, mode='w', newline='') as file:
    writer = csv.writer(file)
    # Scriem capul de tabel
    writer.writerow(["ax", "ay", "az", "gx", "gy", "gz", "temp", "label"])

    try:
        time.sleep(3)
        # Rulam cat timp nu a expirat timpul
        while (time.time() - start_time) < DURATION_SECONDS:
            
            if ser.in_waiting > 0:
                raw_line = ser.readline()
                
                try:
                    text_line = raw_line.decode('utf-8', errors='ignore').strip()
                    data_list = text_line.split(',')
                    
                    # Verificam validitatea datelor (7 valori)
                    if len(data_list) == 7:
                        data_list.append(CURRENT_LABEL) # Adaugam eticheta
                        writer.writerow(data_list)
                        line_count += 1
                        
                        # Afisam progresul la fiecare 50 de linii (ca sa nu spamam consola)
                        if line_count % 50 == 0:
                            elapsed = int(time.time() - start_time)
                            remaining = DURATION_SECONDS - elapsed
                            # Afisare dinamica pe aceeasi linie
                            print(f" Linii: {line_count} | Timp scurs: {elapsed}s | Ramas: {remaining}s   ", end='\r')
                            
                except Exception:
                    pass # Ignoram erorile de decodare ocazionale

    except KeyboardInterrupt:
        print("\n Oprire manuala detectata!")

# --- 4. FINALIZARE ---
ser.close()
total_time = int(time.time() - start_time)
print(f"\n\n GATA! Sesiunea s-a incheiat automat.")
print(f" Timp total: {total_time} secunde")
print(f" Total linii salvate: {line_count}")