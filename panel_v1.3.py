import calendar
import time
import tkinter as tk
import customtkinter as ctk
import serial
from PIL import ImageGrab, ImageFilter, ImageEnhance, ImageTk

PORT = "COM5"

ctk.set_appearance_mode("dark")

ser = None
buf = ""
calisiyor = False
mod = 0            # 0 = kronometre, 1 = zamanlayıcı, 2 = saat
blur_img = None

app = ctk.CTk()
app.geometry("500x690")
app.resizable(False, False)
app.title("ESP32 KRONO")

SOLUK = "#555555"
TURUNCU = "#e0a030"


def gonder(cmd):
    if ser:
        try:
            ser.write((cmd + "\n").encode())
        except (serial.SerialException, OSError):
            baglantiyi_kes()


ctk.CTkLabel(
    app,
    text="ESP32 KRONO",
    font=("Arial", 28, "bold")
).pack(pady=(15, 5))

# ===== CANLI SÜRE =====

sure_label = ctk.CTkLabel(
    app,
    text="00:00:00",
    font=("Consolas", 48, "bold")
)
sure_label.pack(pady=5)

durum_label = ctk.CTkLabel(
    app,
    text="",
    font=("Arial", 16)
)
durum_label.pack(pady=5)

VARSAYILAN_RENK = sure_label.cget("text_color")


# ===== BAŞLAT / DURDUR (TEK BUTON) + SIFIRLA =====

def buton_guncelle():
    if mod == 2:
        toggle_btn.configure(text="▶ BASLAT", fg_color="#3a3a3a")
    elif calisiyor:
        toggle_btn.configure(
            text="⏸ DURDUR",
            fg_color="#c0392b",
            hover_color="#e74c3c"
        )
    else:
        toggle_btn.configure(
            text="▶ BASLAT",
            fg_color="#2e8b57",
            hover_color="#3cb371"
        )


satir1 = ctk.CTkFrame(app, fg_color="transparent")
satir1.pack(pady=5)

toggle_btn = ctk.CTkButton(
    satir1,
    text="▶ BASLAT",
    width=140,
    fg_color="#2e8b57",
    hover_color="#3cb371",
    command=lambda: gonder("TOGGLE")
)
toggle_btn.grid(row=0, column=0, padx=5)

reset_btn = ctk.CTkButton(
    satir1,
    text="🔄 SIFIRLA",
    width=140,
    command=lambda: gonder("RESET")
)
reset_btn.grid(row=0, column=1, padx=5)


# ===== MOD DEĞİŞTİR (KRONOMETRE -> ZAMANLAYICI -> SAAT) =====

def mod_guncelle():
    if mod == 1:
        mod_btn.configure(
            text="⏳ MOD: ZAMANLAYICI",
            fg_color="#d68910",
            hover_color="#e59d2b"
        )
    elif mod == 2:
        mod_btn.configure(
            text="🕒 MOD: SAAT",
            fg_color="#7d3c98",
            hover_color="#9b59b6"
        )
    else:
        mod_btn.configure(
            text="⏱ MOD: KRONOMETRE",
            fg_color="#1f6aa5",
            hover_color="#2a7fc4"
        )


mod_btn = ctk.CTkButton(
    app,
    text="⏱ MOD: KRONOMETRE",
    width=295,
    fg_color="#1f6aa5",
    hover_color="#2a7fc4",
    command=lambda: gonder("MODE")
)
mod_btn.pack(pady=5)


# ===== ZAMANLAYICI SÜRESİ AYARI =====

def ayarla():
    try:
        d = int(dk_entry.get() or 0)
        s = int(sn_entry.get() or 0)
    except ValueError:
        d, s = 1, 0

    d = max(0, min(99, d))
    s = max(0, min(59, s))
    if d == 0 and s == 0:
        s = 1

    dk_entry.delete(0, "end")
    dk_entry.insert(0, str(d))
    sn_entry.delete(0, "end")
    sn_entry.insert(0, f"{s:02d}")

    gonder(f"SETTIME:{d * 60 + s}")


ctk.CTkLabel(app, text="⏲ Zamanlayıcı süresi (dk : sn)", font=("Arial", 13)).pack(pady=(8, 0))

satir2 = ctk.CTkFrame(app, fg_color="transparent")
satir2.pack(pady=4)

dk_entry = ctk.CTkEntry(satir2, width=60, justify="center")
dk_entry.insert(0, "1")
dk_entry.grid(row=0, column=0, padx=3)

ctk.CTkLabel(satir2, text=":", font=("Arial", 20, "bold")).grid(row=0, column=1)

sn_entry = ctk.CTkEntry(satir2, width=60, justify="center")
sn_entry.insert(0, "00")
sn_entry.grid(row=0, column=2, padx=3)

ayarla_btn = ctk.CTkButton(satir2, text="AYARLA", width=90, command=ayarla)
ayarla_btn.grid(row=0, column=3, padx=8)


# ===== TEMALAR =====

satir3 = ctk.CTkFrame(app, fg_color="transparent")
satir3.pack(pady=8)

tema_butonlari = []
for i in (1, 2, 3):
    b = ctk.CTkButton(
        satir3,
        text=f"🎨 TEMA {i}",
        width=90,
        command=lambda i=i: gonder(f"THEME{i}")
    )
    b.grid(row=0, column=i - 1, padx=4)
    tema_butonlari.append(b)


# ===== BUZZER AÇ / KAPA =====

def buzzer_degis():
    gonder("BUZZER1" if buzzer_switch.get() == 1 else "BUZZER0")


buzzer_switch = ctk.CTkSwitch(
    app,
    text="🔔 BUZZER",
    command=buzzer_degis
)
buzzer_switch.select()
buzzer_switch.pack(pady=(8, 5))


# ===== EKRAN PARLAKLIĞI =====

parlak_label = ctk.CTkLabel(app, text="💡 Parlaklık: %100", font=("Arial", 14))
parlak_label.pack(pady=(8, 0))


def parlak_degis(deger):
    v = int(deger)
    parlak_label.configure(text=f"💡 Parlaklık: %{round(v * 100 / 255)}")
    gonder(f"BRIGHT:{v}")


parlak_slider = ctk.CTkSlider(
    app,
    from_=0,
    to=255,
    number_of_steps=51,
    width=250,
    command=parlak_degis
)
parlak_slider.set(255)
parlak_slider.pack(pady=5)


# ===== BLUR KATMANI + ORTADAKİ MESAJ =====

overlay = tk.Label(app, borderwidth=0, bg="#1a1a1a")

mesaj = ctk.CTkFrame(app, fg_color="#1c1c1c", corner_radius=16)
ctk.CTkLabel(mesaj, text="🔌", font=("Arial", 40)).pack(padx=45, pady=(22, 0))
ctk.CTkLabel(
    mesaj,
    text="ESP32'yi bağla",
    font=("Arial", 24, "bold")
).pack(padx=45, pady=(5, 0))
ctk.CTkLabel(
    mesaj,
    text=f"({PORT})",
    font=("Arial", 13),
    text_color=SOLUK
).pack(padx=45, pady=(0, 22))


def blur_goster():
    global blur_img

    if ser is not None:      # arada bağlanmış olabilir
        return

    overlay.place_forget()
    mesaj.place_forget()
    app.update()

    try:
        x = app.winfo_rootx()
        y = app.winfo_rooty()
        w = app.winfo_width()
        h = app.winfo_height()

        img = ImageGrab.grab(bbox=(x, y, x + w, y + h))
        img = img.filter(ImageFilter.GaussianBlur(6))
        img = ImageEnhance.Brightness(img).enhance(0.6)

        blur_img = ImageTk.PhotoImage(img)
        overlay.configure(image=blur_img)
    except Exception:
        overlay.configure(image="")

    overlay.place(x=0, y=0, relwidth=1, relheight=1)
    mesaj.place(relx=0.5, rely=0.5, anchor="center")
    mesaj.lift()


def blur_kaldir():
    overlay.place_forget()
    mesaj.place_forget()


# ===== KİLİT / SOLUK GÖRÜNÜM =====

kontroller = [toggle_btn, reset_btn, mod_btn, *tema_butonlari, buzzer_switch, parlak_slider]


def ayar_durum():
    bagli = ser is not None

    # Süre ayarı sadece bağlıyken ve zamanlayıcı modundayken açık
    d = "normal" if (bagli and mod == 1) else "disabled"
    for w in (dk_entry, sn_entry, ayarla_btn):
        w.configure(state=d)

    # Başlat/durdur ve sıfırla saat modunda pasif
    k = "normal" if (bagli and mod != 2) else "disabled"
    toggle_btn.configure(state=k)
    reset_btn.configure(state=k)


def kilitle(kilitli):
    durum = "disabled" if kilitli else "normal"
    for w in kontroller:
        w.configure(state=durum)

    if kilitli:
        sure_label.configure(text_color=SOLUK)
        parlak_label.configure(text_color=SOLUK)
        durum_label.configure(text="Bağlı değil", text_color=SOLUK)
        toggle_btn.configure(text="▶ BASLAT", fg_color="#3a3a3a")
        mod_btn.configure(fg_color="#3a3a3a")
        app.after(300, blur_goster)
    else:
        blur_kaldir()
        sure_label.configure(text_color=VARSAYILAN_RENK)
        parlak_label.configure(text_color=VARSAYILAN_RENK)
        durum_label.configure(text="Bağlandı", text_color=VARSAYILAN_RENK)
        buton_guncelle()
        mod_guncelle()

    ayar_durum()


# ===== BAĞLANTI =====

def saat_gonder():
    # Bilgisayarın yerel saatini ESP32'ye gönder
    gonder(f"TIME:{calendar.timegm(time.localtime())}")


def saat_dongusu():
    # Bağlıyken 5 dakikada bir saati tekrar eşitle
    if ser:
        saat_gonder()
    app.after(300000, saat_dongusu)


def ilk_ayarlar():
    # ESP32 bağlanınca yeniden başlar; açılışta ayarları tekrar gönder
    buzzer_degis()
    gonder(f"BRIGHT:{int(parlak_slider.get())}")
    ayarla()
    saat_gonder()


def baglan():
    global ser, buf
    try:
        ser = serial.Serial(PORT, 115200, timeout=0)
    except (serial.SerialException, OSError):
        ser = None
        return
    buf = ""
    kilitle(False)
    app.after(2500, ilk_ayarlar)


def baglantiyi_kes():
    global ser, calisiyor, mod
    if ser:
        try:
            ser.close()
        except Exception:
            pass
    ser = None
    calisiyor = False
    mod = 0
    sure_label.configure(text="00:00:00")
    kilitle(True)


def kontrol():
    # Bağlı değilse her saniye tekrar dene
    if ser is None:
        baglan()
    app.after(1000, kontrol)


# ===== ESP32'DEN GELEN VERİ =====

def isle(line):
    global calisiyor, mod

    if not line.startswith("T:"):
        return

    parcalar = line.split(":")
    if len(parcalar) != 6:
        return
    try:
        ms = int(parcalar[1])
        st = parcalar[2]
        yeni_mod = int(parcalar[3])
        bitti = parcalar[4] == "1"
        saat_ok = parcalar[5] == "1"
    except ValueError:
        return

    if yeni_mod == 2:
        # SAAT modu: ms = günün saniyesi * 1000
        if saat_ok:
            sa = ms // 3600000
            dk = (ms // 60000) % 60
            sn = (ms // 1000) % 60
            metin = f"{sa:02d}:{dk:02d}:{sn:02d}"
            durum_label.configure(text="🕒 Saat", text_color=VARSAYILAN_RENK)
        else:
            metin = "--:--:--"
            durum_label.configure(text="Saat ayarlanmadı", text_color=TURUNCU)
        sure_label.configure(text=metin, text_color=VARSAYILAN_RENK)
    else:
        dakika = ms // 60000
        saniye = (ms // 1000) % 60
        salise = (ms % 1000) // 10

        sure_label.configure(
            text=f"{dakika:02d}:{saniye:02d}:{salise:02d}",
            text_color=TURUNCU if bitti else VARSAYILAN_RENK
        )

        if bitti:
            durum_label.configure(text="⏰ BİTTİ!", text_color=TURUNCU)
        else:
            durum_label.configure(
                text="Çalışıyor" if st == "1" else "Durdu",
                text_color=VARSAYILAN_RENK
            )

    yeni = (st == "1")
    degisti = False

    if yeni != calisiyor:
        calisiyor = yeni
        degisti = True

    if yeni_mod != mod:
        mod = yeni_mod
        mod_guncelle()
        ayar_durum()
        degisti = True

    if degisti:
        buton_guncelle()


def oku():
    global buf
    if ser:
        try:
            n = ser.in_waiting
            if n:
                buf += ser.read(n).decode(errors="ignore")
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    isle(line.strip())
        except (serial.SerialException, OSError):
            baglantiyi_kes()
    app.after(30, oku)


def kapat():
    if ser:
        try:
            ser.close()
        except Exception:
            pass
    app.destroy()


# ===== BAŞLANGIÇ =====

kilitle(True)
baglan()

app.protocol("WM_DELETE_WINDOW", kapat)
app.after(30, oku)
app.after(1000, kontrol)
app.after(300000, saat_dongusu)
app.mainloop()