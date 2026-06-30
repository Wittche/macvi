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

## Faz 15'in Tamamlanması (Gelişmiş GDI, Timer ve Kontroller)
Faz 15 başarıyla tamamlandı. Artık MacWI;
- `CreateFontA` ile macOS CoreText altyapısı üzerinden metinleri render edebiliyor.
- `BUTTON`, `STATIC` ve `EDIT` sistem sınıflarını Native Cocoa/GDI karışık bir şekilde ekranda gösterebiliyor.
- `SetTimer` ve asenkron `WM_TIMER` entegrasyonu sayesinde frame bazlı oyun döngülerini (animasyon vb.) JIT'i bloke etmeden çalıştırabiliyor.

## Bundan Sonra Nasıl İlerleyeceğiz? (Faz 16 ve Sonrası)

### Adım 1: Gelişmiş GDI Çizimleri ve Bitmapler
- Cihazdan bağımsız Bitmap (DIB Section) implementasyonu.
- Görüntü (Bitmap) kopyalama, Alpha blending ve karmaşık GUI nesnelerinin desteklenmesi.

### Adım 2: Gelişmiş Input (Mouse & Klavye)
- Klavye tuşlarının (Virtual Keys) birebir çevirisi ve `WM_KEYDOWN`/`WM_KEYUP` eventlerinin zenginleştirilmesi.
- Çoklu monitör desteği ve pencere pozisyonlama mantığının (SetWindowPos) iyileştirilmesi.

### Adım 3: Thunk Katmanında Hızlandırma (Fexi Evrimi)
- Fexi Thunk yapısının yeniden elden geçirilmesi.
- Host tarafına sık düşen (örneğin GetMessage, çizim) fonksiyonları için Custom IR thunk'ların yazılarak (JIT içi) context-switch maliyetlerinin düşürülmesi.

## Neden Bu Yolu Seçiyoruz?
Şu anda işletim sisteminin "kalbi" (CPU, Memory, File System, Threading) ve temel "arayüzü" (Window, Message Loop, Basic Paint) sağlıklı çalışmaktadır. Ancak karmaşık bir Windows oyunu veya programı, basit metinlerden çok daha fazlasına (zamanlayıcılar, bitmapler, input focus vb.) ihtiyaç duyar. İleri grafik ve timer yetenekleri kazandığımızda, eski nesil 32-bit oyunları ve programları macOS ekranında görsel olarak tam teşekküllü render etmeye bir adım daha yaklaşmış olacağız.
