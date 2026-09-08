import pandas as pd
import os
import glob
import re

# --- CONFIGURARE ---
# Sesiunea 1 (dec. 2025). Pentru sesiunea a doua: "date_raw_2026_06"
MASTER_DIRECTORY = "date_raw_2025_12"
OUTPUT_FILE = "dataset_final.csv"

LABEL_MAP = {
    'zgomot': 0,       # Clasa 0
    'normal': 1,       # Clasa 1
    'dezechilibru': 2, # Clasa 2
    'frecare': 3,      # Clasa 3
    'fixare': 4        # Clasa 4
}

def procesare_unificare():
    all_dataframes = []
    
    files = glob.glob(os.path.join(MASTER_DIRECTORY, "*.csv"))
    
    if not files:
        print("EROARE: Nu am gasit niciun fisier .csv! Verifica folderul.")
        return

    for file_path in files:
        filename = os.path.basename(file_path).lower()
        
        try:
            df = pd.read_csv(file_path)
            df.columns = [c.strip() for c in df.columns]
            
            # 3. DETERMINAM ETICHETA (LABEL) DIN NUMELE FISIERULUI
            # Asta e partea critica: suprascriem coloana 'label' cu ce scrie in nume
            current_label = None
            
            for key, val in LABEL_MAP.items():
                if key in filename:
                    current_label = val
                    break
            
            if current_label is not None:
                df['label'] = current_label
            else:
                print(f"ATENTIE: Nu pot deduce eticheta pentru '{filename}'. Lasam originalul.")

            speed_match = re.search(r'(\d+)', filename)
            if speed_match:
                df['pwm_speed'] = int(speed_match.group(1))
            else:
                df['pwm_speed'] = 0 
            all_dataframes.append(df)
            print(f" Adaugat: {filename} -> Label: {current_label} | Linii: {len(df)}")
            
        except Exception as e:
            print(f"Eroare la citirea {filename}: {e}")

    if all_dataframes:
        print("\n Unificam toate datele...")
        master_df = pd.concat(all_dataframes, ignore_index=True)
        
        master_df.to_csv(OUTPUT_FILE, index=False)
        
        print("="*50)
        print(f" SUCCES! Dataset salvat in: {OUTPUT_FILE}")
        print(f" Total Linii: {len(master_df)}")
        print("\nDistributia pe clase:")
        print(master_df['label'].value_counts().sort_index())
        print("="*50)
    else:
        print("Nu am reusit sa procesez niciun fisier.")

if __name__ == "__main__":
    procesare_unificare()