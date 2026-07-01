# MacWI Gelecek Yol Haritası ve Planı (Phase 25)

Şu ana kadar MacWI projesinin temel omurgasını oluşturan tüm "çekirdek" (Core) bileşenlerini tamamladık:
1. **PE Loader**: 32-bit EXE yükleme, hafızaya haritalama, IAT çözme.
2. **JIT Emülatör (FEXCore)**: ARM64 üzerinde x86 komut seti çevirisi ve donanım izolasyonu.
3. **Thunking Katmanı**: 32-bit guest ile 64-bit host arasındaki veri aktarımı ve API yönlendirmesi.
4. **Sistem Çağrıları ve Senkronizasyon (Kernel32)**: Multithreading, süreç yönetimi, dosya yönetimi.
5. **Dosya Sistemi (VFS - Phase 23)**: POSIX dizin yapısıyla senkronize çalışan, Windows tarzı `C:\Windows` path yönlendirmeleri.
6. **Bellek Yönetimi (Memory - Phase 24)**: VirtualAlloc, VirtualFree, VirtualProtect ve VirtualQuery ile tam uyumlu 32-bit bellek yönetimi.

Artık MacWI'nin altyapısı "arkaplanda" (headless) çalışan hemen hemen tüm konsol uygulamalarını koşturabilecek seviyededir.

## Bundan Sonra Nasıl İlerleyeceğiz? (Phase 25 ve Sonrası)

MacWI'yi gerçek bir "Windows uyumluluk katmanı" (Wine benzeri) yapabilmek için atılması gereken en büyük adımlardan olan **Grafik ve Kullanıcı Arayüzü (User32 ve GDI32)** entegrasyonuna (Phase 25) geçiyoruz. 

### Adım 1: USER32 / Pencere Yönetimi (Phase 25 - Aktif)
- `RegisterClassExA`, `CreateWindowExA` ve `DefWindowProcA` ile native macOS Cocoa pencereleri (NSWindow) oluşturulacak.
- `GetMessageA`, `TranslateMessage`, `DispatchMessageA` ile Windows Message Loop altyapısı Cocoa'nın event-loop'una entegre edilecek.
- Fare (Mouse) ve Klavye (Keyboard) olayları (WM_KEYDOWN, WM_MOUSEMOVE) emüle edilecek.

### Adım 2: Grafikler (D3D9 -> MoltenVK - Phase 26)
- DirectX 9 `CreateDevice`, `BeginScene`, `EndScene`, `Present` vb. API çağrıları Vulkan/Metal komutlarına (MoltenVK kullanarak) çevrilecek.
- Donanım hızlandırmalı grafik çizimleri (Hardware Accelerated Graphics) entegrasyonu tamamlanacak.

### Adım 3: Gelişmiş DLL'ler ve Ağ (Phase 27+)
- `ws2_32.dll` ile Winsock altyapısı BSD Sockets altyapısına bağlanarak online oyun / internet bağlantısı özellikleri sağlanacak.
- `dsound.dll` veya `xaudio2` ile CoreAudio kullanılarak ses emülasyonu eklenecek.

## Neden Bu Yolu Seçiyoruz?
Şu anda işletim sisteminin "kalbi" (CPU, Memory, File System, Threading) sağlıklı çalışmaktadır. Tam donanımlı Windows oyunlarını çalıştırabilmek için en acil ihtiyacımız, bir pencere açabilmek (USER32) ve bu pencereye donanım destekli çizim (DirectX 9) yapabilmektir. Bu nedenle Phase 25'ten itibaren tamamen GUI odaklı bir ilerleme sağlayacağız.
