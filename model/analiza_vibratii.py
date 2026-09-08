import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft, fftfreq

LABELS_MAP = {1: 'NORMAL', 2: 'DEZECHILIBRAT', 3: 'FRECARE', 4: 'SLABIT'}


def incarca_si_filtreaza(filepath):
    print(">>> Începem analiza matematică a datelor...\n")
    df = pd.read_csv(filepath)

    # 1. Selectăm o viteză comună pentru comparație
    # (Nu comparăm motor turat maxim cu motor la relanti)
    speeds = df[df['pwm_speed'] > 100]['pwm_speed'].value_counts()
    if speeds.empty:
        print("Nu am găsit date cu viteză > 100. Verifică coloana pwm_speed.")
        return None, None

    target_speed = speeds.index[0]  # Luăm cea mai comună viteză
    print(f"Analizăm datele la viteza PWM: {target_speed}")
    print("-" * 40)

    return df[df['pwm_speed'] == target_speed].copy(), target_speed


def tabel_scoruri(df_filtered):
    # 2. Tabel de Scoruri (Aici e adevărul)
    results = []

    for label_id in [1, 2, 3, 4]:
        subset = df_filtered[df_filtered['label'] == label_id]

        if len(subset) < 10:
            continue

        # Calculăm Deviația Standard (Cât de mult variază semnalul față de medie)
        # Asta este practic "Amplitudinea Vibrației"
        std_ax = subset['ax'].std()
        std_ay = subset['ay'].std()
        std_az = subset['az'].std()

        # Scor total (Vector sum)
        total_vibration = np.sqrt(std_ax**2 + std_ay**2 + std_az**2)

        results.append({
            'Stare': LABELS_MAP.get(label_id, f'Label {label_id}'),
            'Vibrație X (std)': round(std_ax, 4),
            'Vibrație Y (std)': round(std_ay, 4),
            'Vibrație Z (std)': round(std_az, 4),
            'SCOR TOTAL VIBRAȚIE': round(total_vibration, 4)
        })

    # Creăm un DataFrame pentru afișare frumoasă
    df_results = pd.DataFrame(results)
    print(df_results)
    print("-" * 40)
    return df_results


def concluzie(df_results):
    # Interpretare automată
    print("\n>>> CONCLUZIE PRELIMINARĂ:")
    normal_score = df_results[df_results['Stare'] == 'NORMAL']['SCOR TOTAL VIBRAȚIE'].values
    if len(normal_score) > 0:
        normal_val = normal_score[0]
        print(f"Baza (Normal) este: {normal_val}")

        for index, row in df_results.iterrows():
            if row['Stare'] != 'NORMAL':
                diff = row['SCOR TOTAL VIBRAȚIE'] / normal_val
                print(f" -> {row['Stare']} este de {diff:.2f}x mai zgomotos decât Normal.")
                if diff > 1.2:
                    print("    (Diferență detectabilă! AI-ul o va vedea ușor)")
                else:
                    print("    (Diferență mică. S-ar putea să fie nevoie de FFT sau Time-VQ-VAE)")


def plot_semnal_brut(df_filtered, target_speed):
    # 3. Vizualizare "Zoom In" (Fără Gravitație)
    # Aici ar trebui să se vadă clar diferența
    plt.figure(figsize=(14, 6))

    colors = {1: 'green', 2: 'red', 3: 'orange', 4: 'blue'}

    for label_id in [1, 2, 4]:  # Comparăm Normal vs 2 defecte
        subset = df_filtered[df_filtered['label'] == label_id]
        if subset.empty: continue

        # Luăm o mostră de 200 puncte
        sample = subset['ax'].iloc[500:700].values

        # SCOATEM MEDIA (GRAVITAȚIA) -> Centrare pe 0
        sample = sample - np.mean(sample)

        plt.plot(sample, label=f"{LABELS_MAP.get(label_id)} (Std: {subset['ax'].std():.3f})",
                 color=colors.get(label_id, 'black'), alpha=0.7)

    plt.title(f"Vibrația REALĂ (Gravitație eliminată) la PWM {target_speed}", fontsize=14)
    plt.ylabel("Amplitudine (g)")
    plt.xlabel("Timp (eșantioane)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.show()


def plot_spectru_fft(df_filtered):
    print("\n" + "="*50)
    print("GENERARE ANALIZĂ FRECVENȚĂ (FFT) - Pentru defecte subtile")
    print("="*50)

    # Vom salva datele pentru FFT
    fft_data = {}
    for label_id in [1, 2, 3, 4]:
        subset = df_filtered[df_filtered['label'] == label_id]
        if len(subset) < 100: continue

        # Păstrăm datele pentru graficul FFT (pe axa X - cea mai relevantă de obicei)
        # Luăm primele 1000 de puncte pentru consistență
        signal_sample = subset['ax'].values[:1000]
        if len(signal_sample) == 1000:
            fft_data[label_id] = signal_sample

    # 3. Vizualizare FFT (Spectrul de Frecvență)
    # Aici căutăm diferențele pentru Frecare și Slăbit
    if 1 in fft_data:
        plt.figure(figsize=(14, 8))

        # Parametri FFT estimativi
        N = 1000
        SAMPLE_RATE = 100  # Hz (Estimativ pentru MPU6050)
        xf = fftfreq(N, 1 / SAMPLE_RATE)[:N//2]

        # Plotăm Normalul ca referință (umbră)
        yf_normal = fft(fft_data[1] - np.mean(fft_data[1]))
        amp_normal = 2.0/N * np.abs(yf_normal[0:N//2])
        plt.plot(xf, amp_normal, label='NORMAL (Referință)', color='green', alpha=0.3, linewidth=2)
        plt.fill_between(xf, 0, amp_normal, color='green', alpha=0.1)

        colors = {2: 'red', 3: 'orange', 4: 'blue'}

        # Plotăm defectele
        for lbl, data in fft_data.items():
            if lbl == 1: continue  # Deja plotat

            yf = fft(data - np.mean(data))
            amp = 2.0/N * np.abs(yf[0:N//2])

            stare_nume = LABELS_MAP.get(lbl, str(lbl))
            plt.plot(xf, amp, label=f'{stare_nume}', color=colors.get(lbl, 'black'), alpha=0.8)

        plt.title(f"Amprenta Spectrală (FFT) - De ce avem nevoie de AI?", fontsize=16)
        plt.xlabel("Frecvență (Hz)", fontsize=12)
        plt.ylabel("Amplitudine", fontsize=12)
        plt.legend()
        plt.grid(True)
        plt.show()
        print("Interpretează graficul: Dacă liniile colorate au vârfuri unde linia verde NU are, AI-ul va detecta asta!")


def calculeaza_scor_vibratie(filepath):
    df_filtered, target_speed = incarca_si_filtreaza(filepath)
    if df_filtered is None:
        return

    df_results = tabel_scoruri(df_filtered)
    plot_semnal_brut(df_filtered, target_speed)
    concluzie(df_results)
    plot_spectru_fft(df_filtered)


if __name__ == "__main__":
    try:
        calculeaza_scor_vibratie('dataset_final.csv')
    except Exception as e:
        print(f"Eroare: {e}")
