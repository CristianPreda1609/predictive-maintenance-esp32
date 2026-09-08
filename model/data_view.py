import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

# Setări pentru grafice mai frumoase
sns.set(style="whitegrid")
plt.rcParams['figure.figsize'] = (15, 10)

def analizeaza_datele(filepath):
    # 1. Încărcare date
    print("Se încarcă datele...")
    df = pd.read_csv(filepath)
    
    # Afișăm primele rânduri să fim siguri
    print("Primele 5 rânduri:")
    print(df.head())
    
    # 2. Verificarea Balanței Datelor (Câte rânduri per Label)
    plt.figure(figsize=(10, 6))
    ax = sns.countplot(x='label', data=df, palette='viridis')
    plt.title('Distribuția Datelor pe Categorii (Clase)', fontsize=16)
    plt.xlabel('Label (0=Oprit, 1=Normal, 2=Dezechilibrat, 3=Frecare, 4=Slab)', fontsize=12)
    plt.ylabel('Număr de citiri', fontsize=12)
    
    # Adăugăm etichetele de text (numele defectelor)
    labels_map = {0: 'Oprit', 1: 'Normal', 2: 'Dezechilibrat', 3: 'Frecare', 4: 'Slab Montat'}
    # Ajustăm axa X să arate numele, nu doar numerele
    ax.set_xticklabels([labels_map.get(int(x.get_text()), x.get_text()) for x in ax.get_xticklabels()])
    
    plt.tight_layout()
    plt.show()
    
    print("-" * 50)
    print("Statistici pe clase:")
    print(df['label'].value_counts().sort_index())
    print("-" * 50)

    # 3. Vizualizare Comparativă: Cum arată Accelerometrul X pentru fiecare stare?
    # Luăm o mostră de 200 de puncte din fiecare clasă (ca să nu aglomerăm graficul)
    features_to_plot = ['ax', 'gz'] # Unul de accelerație, unul de giroscop
    
    fig, axes = plt.subplots(len(features_to_plot), 1, figsize=(15, 10))
    
    for i, feature in enumerate(features_to_plot):
        for label_id in [1, 2, 4]: # Comparăm Normal vs Dezechilibrat vs Slab Montat
            subset = df[df['label'] == label_id]
            if not subset.empty:
                # Luăm o fereastră arbitrară din mijlocul înregistrării
                start_idx = len(subset) // 2
                end_idx = start_idx + 150 # Vedem 150 de eșantioane
                data_slice = subset.iloc[start_idx:end_idx][feature].reset_index(drop=True)
                
                axes[i].plot(data_slice, label=f'{labels_map[label_id]}', alpha=0.8, linewidth=2)
        
        axes[i].set_title(f'Comparație Semnal: {feature.upper()} (Senzor)', fontsize=14)
        axes[i].legend()
        axes[i].set_xlabel('Timp (Eșantioane)')
        axes[i].set_ylabel('Valoare Senzor')

    plt.tight_layout()
    plt.show()

    # 4. Demonstrație Smoothing (De ce ai nevoie de filtrare)
    # Luăm un eșantion "zgomotos" de la motor dezechilibrat (Label 2)
    sample_noisy = df[df['label'] == 2]['ax'].iloc[1000:1200].reset_index(drop=True)
    
    # Aplicăm filtrul (Moving Average)
    sample_smooth = sample_noisy.rolling(window=10).mean()
    
    plt.figure(figsize=(15, 6))
    plt.plot(sample_noisy, label='Date Brute (RAW)', color='lightgray', linestyle='--')
    plt.plot(sample_smooth, label='Date Filtrate (Smoothing window=10)', color='red', linewidth=2)
    plt.title('Efectul Filtrării (Smoothing) - Esențial pentru ESP32', fontsize=16)
    plt.legend()
    plt.show()

# Rulează funcția (Asigură-te că fișierul e în același folder sau pune calea completă)
try:
    analizeaza_datele('dataset_final.csv')
except FileNotFoundError:
    print("Eroare: Nu găsesc fișierul 'dataset_final.csv'. Verifică numele sau calea.")