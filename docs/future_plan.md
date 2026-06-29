# MacWI Gelecek Yol Haritası ve Faz 12 Planı

Şu ana kadar (Faz 1'den Faz 11'e kadar) MacWI projesinin temel omurgasını oluşturan tüm "çekirdek" (Core) bileşenlerini tamamladık:
1. **PE Loader**: 32-bit EXE yükleme, hafızaya haritalama, IAT çözme.
2. **JIT Emülatör (FEXCore)**: ARM64 üzerinde x86 komut seti çevirisi ve donanım izolasyonu.
3. **Thunking Katmanı**: 32-bit guest ile 64-bit host arasındaki veri aktarımı ve API yönlendirmesi.
4. **Sistem Çağrıları ve Senkronizasyon (Kernel32)**: Multithreading (CreateThread, Mutex), in-memory Registry (Advapi32) ve süreç yönetimi (ExitProcess).
5. **Dosya Sistemi (VFS)**: POSIX dizin yapısıyla senkronize çalışan, Windows tarzı `C:\Windows` path yönlendirmeleri ve dizin iterasyonu (FindFirstFile).

Artık MacWI'nin altyapısı "arkaplanda" (headless) çalışan hemen hemen tüm konsol uygulamalarını koşturabilecek seviyededir.

## Bundan Sonra Nasıl İlerleyeceğiz? (Faz 15 ve Sonrası)

MacWI'yi gerçek bir "Windows uyumluluk katmanı" (Wine benzeri) yapabilmek için atılması gereken en büyük adımlardan olan **Grafik ve Kullanıcı Arayüzü (User32 ve GDI32)** entegrasyonu (Faz 12-14) başarıyla tamamlandı. Artık MacWI, ekranda native macOS pencereleri (NSWindow) oluşturabiliyor, event döngülerini (Message Loop) yönetebiliyor ve `WM_PAINT` sırasında senkronize GDI çizimlerini (`FillRect`, `TextOutA`) yapabiliyor.

Bu bağlamda **Faz 15 ve Sonrası (Gelişmiş Grafik ve Kontroller)** için planımız şu şekildedir:

### Adım 1: Gelişmiş GDI Çizimleri ve Fontlar
- `gdi32.dll` thunk'larının genişletilmesi.
- Özel Fontların (CreateFontA) CoreText üzerinden render edilmesi.
- Görüntü (Bitmap) kopyalama, Alpha blending ve karmaşık GUI nesnelerinin desteklenmesi.

### Adım 2: Pencere İçi Kontroller (Common Controls)
- Buton, Textbox, Listbox, Combobox vb. klasik Windows UI bileşenlerinin (Common Controls) emüle edilmesi.
- Bu bileşenlerin event'lerinin (Command Messages) ana pencereye yönlendirilmesi.

### Adım 3: Zamanlayıcılar (Timers)
- `SetTimer` ve `KillTimer` fonksiyonlarının implemente edilerek `WM_TIMER` event'lerinin message loop üzerinden işlenmesi. Animasyonlar ve periyodik UI güncellemeleri için şarttır.

### Adım 4: Gelişmiş Mouse & Klavye İşlemleri
- Klavye tuşlarının (Virtual Keys) birebir çevirisi ve `WM_KEYDOWN`/`WM_KEYUP` eventlerinin zenginleştirilmesi.
- Çoklu monitör desteği ve pencere pozisyonlama mantığının (SetWindowPos) iyileştirilmesi.

## Neden Bu Yolu Seçiyoruz?
Şu anda işletim sisteminin "kalbi" (CPU, Memory, File System, Threading) ve temel "arayüzü" (Window, Message Loop, Basic Paint) sağlıklı çalışmaktadır. Ancak karmaşık bir Windows oyunu veya programı, basit metinlerden çok daha fazlasına (zamanlayıcılar, bitmapler, input focus vb.) ihtiyaç duyar. İleri grafik ve timer yetenekleri kazandığımızda, eski nesil 32-bit oyunları ve programları macOS ekranında görsel olarak tam teşekküllü render etmeye bir adım daha yaklaşmış olacağız.
