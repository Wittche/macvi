# MacWI Gelecek Yol Haritası ve Faz 12 Planı

Şu ana kadar (Faz 1'den Faz 11'e kadar) MacWI projesinin temel omurgasını oluşturan tüm "çekirdek" (Core) bileşenlerini tamamladık:
1. **PE Loader**: 32-bit EXE yükleme, hafızaya haritalama, IAT çözme.
2. **JIT Emülatör (FEXCore)**: ARM64 üzerinde x86 komut seti çevirisi ve donanım izolasyonu.
3. **Thunking Katmanı**: 32-bit guest ile 64-bit host arasındaki veri aktarımı ve API yönlendirmesi.
4. **Sistem Çağrıları ve Senkronizasyon (Kernel32)**: Multithreading (CreateThread, Mutex), in-memory Registry (Advapi32) ve süreç yönetimi (ExitProcess).
5. **Dosya Sistemi (VFS)**: POSIX dizin yapısıyla senkronize çalışan, Windows tarzı `C:\Windows` path yönlendirmeleri ve dizin iterasyonu (FindFirstFile).

Artık MacWI'nin altyapısı "arkaplanda" (headless) çalışan hemen hemen tüm konsol uygulamalarını koşturabilecek seviyededir.

## Bundan Sonra Nasıl İlerleyeceğiz? (Faz 12 ve Sonrası)

MacWI'yi gerçek bir "Windows uyumluluk katmanı" (Wine benzeri) yapabilmek için atılması gereken bir sonraki en büyük adım **Grafik ve Kullanıcı Arayüzü (User32 ve GDI32)** entegrasyonudur.

Bu bağlamda **Faz 12 (Grafik Subsystem)** için planımız şu şekildedir:

### Adım 1: Temel User32.dll Entegrasyonu ve Pencere Sınıfları
- `user32.dll` thunk altyapısının projeye dahil edilmesi.
- `RegisterClassExA` ve `CreateWindowExA` gibi temel pencere oluşturma API'lerinin macOS native pencere sistemine (Cocoa / AppKit) bağlanması.
- 32-bit uygulamaların `HWND` (Pencere Handle'ları) değerlerinin, arka planda oluşturulan `NSWindow` referanslarına map edilmesi.

### Adım 2: Mesaj Döngüsü (Message Loop)
- Windows uygulamalarının can damarı olan `GetMessageA`, `TranslateMessage`, ve `DispatchMessageA` fonksiyonlarının implementasyonu.
- macOS event loop'unun (NSEvent) dinlenmesi ve tıklama, klavye girişi, pencere kapatma gibi olayların Windows spesifik `WM_PAINT`, `WM_QUIT`, `WM_LBUTTONDOWN` mesajlarına çevrilerek guest uygulamaya iletilmesi.

### Adım 3: Temel Çizim İşlemleri (GDI32)
- Bir pencere içine bir şeyler çizebilmek için `gdi32.dll` thunk'larının oluşturulması.
- `BeginPaint`, `EndPaint`, `GetDC` API'lerinin macOS `CGContext` (Core Graphics) veya `Metal` API'lerine bağlanması.
- Ekrana basit metinler (TextOut) ve düz renkli kutular (FillRect) çizdirerek grafik köprüsünün doğrulanması.

### Adım 4: Gelişmiş GUI Test Uygulaması (gui_win32.exe)
- Konsol testlerimizin (`fs_win32.exe`, `advanced_win32.exe`) yanına bir de `gui_win32.exe` test uygulamasının yazılması.
- Bu uygulamanın ekranda basit bir pencere açması, bir mesaj döngüsü çalıştırması ve kullanıcı pencereyi kapatana kadar ekranda bir metin veya şekil göstermesi.

## Neden Bu Yolu Seçiyoruz?
Şu anda işletim sisteminin "kalbi" (CPU, Memory, File System, Threading) tamamen sağlıklı çalışmaktadır. Ancak bir Windows uygulamasının varoluş amacı görsel arayüz sunmaktır. `user32.dll` entegrasyonu olmadan MacWI sadece arka plan görevlerini çalıştırabilen bir terminal emülatörü olarak kalır. Pencere çizdirme altyapısını kurduğumuz anda, eski nesil 32-bit oyunları ve programları macOS ekranında görsel olarak render etmeye bir adım daha yaklaşmış olacağız.
