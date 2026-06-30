# MacWI (Mac Windows Interface) Proje Dokümantasyonu

MacWI, Apple Silicon (ARM64) tabanlı macOS sistemlerinde 32-bit (x86) Windows uygulamalarını çalıştırmayı hedefleyen WoW64 benzeri bir uyumluluk katmanıdır. Temel hedefi **Wine projesine güçlü bir alternatif olmak** ve Windows API'lerini (kernel32, ntdll vb.) doğrudan macOS eşdeğerleriyle (POSIX, Cocoa vb) değiştirerek (wrapper) yüksek performanslı bir kullanıcı deneyimi sunmaktır.

Projenin felsefesi: *"Mimariyi, sonra kullanıcı deneyimini çöz."*

## 1. Mimari Genel Bakış

MacWI, saf bir emülatör değildir. Yalnızca x86 komut setini ARM64'e çevirmek için yüksek performanslı bir JIT (Just-In-Time) derleyicisi kullanır. Geri kalan tüm Windows API çağrıları, emülatör dışına çıkartılarak (thunking) host (macOS) üzerinde native olarak koşturulur. Bu, WoW64'ün (Windows on Windows 64-bit) çalışma mantığına çok benzer.

MacWI temel olarak dört ana katmandan oluşur:

1. **PE Loader (Yükleyici):** Windows `.exe` ve `.dll` dosyalarını (PE formatı) okur, ayıklar, hafızaya yerleştirir ve IAT (Import Address Table) tablolarını dinamik olarak yamalar.
2. **JIT Emülatör (FEXCore):** x86 komutlarını ARM64'e anlık olarak çevirir. CPU register'larını ve thread state'lerini (durumlarını) yönetir. 
3. **Thunking Katmanı:** 32-bit emülatör dünyası ile 64-bit native macOS dünyası arasındaki veri ve parametre köprüsüdür. Pointer dönüşümlerini, stack okumalarını ve API yönlendirmelerini yapar.
4. **Win32 API Implementasyonu:** Windows fonksiyonlarının macOS üzerindeki gerçek karşılıklarının yazıldığı yerdir (Örn: `kernel32.dll` -> `GetTickCount`, `Sleep` vb.)

## 2. Kullanılan Teknolojiler ve Çekirdek (FEXCore)

MacWI, x86 -> ARM64 çevirisi için **FEXCore** (FEX-Emu projesinin çekirdek kütüphanesi) kullanır. Başlangıçta projeye Unicorn emülatörü ile başlanmış, ancak Unicorn'un yorumlayıcı (interpreter) tabanlı yapısı ve düşük performansı nedeniyle, production-ready bir JIT motoru olan FEXCore'a geçiş yapılmıştır. 

FEXCore **sadece bir çevirici** olarak kullanılır. Linux işletim sistemi emülasyonu yapan kısımları tamamen devreden çıkartılmış, kendi `SyscallHandler` yapımızla (kesme dinleyicimizle) değiştirilmiştir.

## 3. Çalışma Mantığı ve "Thunk" Süreci

Bir Windows programı çalıştırıldığında şu adımlar izlenir:

### A. PE Yükleme ve Hafıza Hazırlığı
1. Kullanıcı `macwi hello_win32.exe` komutunu çalıştırır.
2. **PE Loader**, exe dosyasını ayrıştırır. Exe'nin import ettiği tüm dış fonksiyonları (örneğin `Sleep`) tespit eder.
3. Tespit edilen her bir fonksiyon için, **Thunk Katmanı** hafızada rastgele çalıştırılabilir bir bölgeye küçük bir Assembly kodu yazar (Buna **Trambolin / Trampoline** denir).

### B. Trambolin (Trampoline) Yapısı
Yazılan assembly kodu şu şekildedir:
```nasm
mov eax, <API_INDEX_NUMARASI>
int 0x80
ret N
```
PE Loader, IAT (Import Address Table) içerisindeki `Sleep` fonksiyonunu gösteren pointer'ı silip, bizim hafızada ürettiğimiz bu trambolinin adresini yazar. Yani `.exe`, `Sleep` fonksiyonunu çağırdığını sanırken aslında bizim assembly kodumuzu çağırır.

### C. Sistem Çağrısı ve Çalıştırma (Execution)
1. FEXCore JIT derleyicisi çalışmaya başlar. Emülatör tramboline ulaştığında `int 0x80` kesmesini (interrupt) görür.
2. FEXCore bu kesmeyi görür görmez emülasyonu anlık olarak durdurur ve MacWI içerisine yazdığımız `MacWISyscallHandler::HandleSyscall` fonksiyonunu çağırır.
3. MacWI, FEXCore'un `CpuStateFrame` nesnesine erişerek o anki register durumunu alır. `EAX` register'ında bulunan numaradan hangi fonksiyonun çağrıldığını anlar (Örn: EAX = 1, demek ki Sleep çağrıldı).
4. `macwi_thunk_read_param_32` fonksiyonu, guest ESP (Stack Pointer) üzerinden `stdcall` mimarisine uygun şekilde argümanları (örneğin milisaniye değerini) okur.
5. MacWI host tarafında native olarak kendi yazdığımız C++ `my_Sleep()` fonksiyonunu çalıştırır.
6. İşlem bitince return değeri (dönüş sonucu) guest `EAX` register'ına yazılır. FEXCore emülasyona geri döner, `ret N` komutunu çalıştırarak stack'i temizler ve Windows programı kaldığı yerden devam eder.

## 4. Şimdiye Kadar Tamamlanan Aşamalar (Fazlar)

Projenin yol haritasında %100 başarıyla tamamlanan aşamalar şunlardır:

* **Faz 1: Temel Kurulum:** CMake build sistemi, projenin `core`, `loader`, `emu`, `thunk` ve `win32` modüllerine ayrılması.
* **Faz 2: PE Loader Çekirdeği:** DOS Header, PE File Header, Optional Header ve Section Header okuyan, dosyayı hafızaya mmap ile alan loader modülünün yazılması.
* **Faz 3: Unicorn Emülatörü:** İlk prototip için Unicorn kütüphanesinin projeye dahil edilmesi (Daha sonra iptal edildi).
* **Faz 4: Temel Thunking:** 32-bit pointer'lardan 64-bit native yapılara veri dönüştürme mantığının tasarlanması.
* **Faz 5: PE & Emülatör Birleşimi:** Hafızaya map edilen bölümlerin emülatöre tanıtılması.
* **Faz 6: FEXCore Geçişi:** Unicorn'un atılarak, FEXCore kaynak kodunun CMake ile projeye tam statik build olarak eklenmesi.
* **Faz 7: FEXCore Stabilite Yamaları:** macOS üzerindeki 48-bit Virtual Address kısıtlamalarının aşılması, MAP_FIXED yerine native mmap fallback sisteminin kurulması.
* **Faz 8: API Yüzeyi ve Thunk Kesmeleri:** FEXCore `SyscallHandler` entegrasyonu, Trambolin (Trampoline) mimarisinin başarıyla çalıştırılması. Stack üzerinden 32-bit parametre okuma işlemlerinin senkronize edilmesi ve `test_thunk_dispatch.c` üzerinde kusursuz `int 0x80` testinin geçilmesi.
* **Faz 9: End-to-End Win32 PE Çalıştırma:** Sıfırdan bir `hello_win32.exe` Windows programını loader ile yükleyip, `kernel32` importlarını trambolinlere bağlayıp, baştan sona eksiksiz bir şekilde çalıştırma aşaması başarıyla tamamlandı. `ExitProcess` çağrılarına kadar sistem sorunsuz test edildi.
* **Faz 10: Sistem Çağrıları, Threading ve Registry:**
  * `CreateThread`, `ExitThread` ve `GetCurrentThreadId` ile pthreads tabanlı Win32 Thread oluşturma altyapısı kuruldu (FEXCore TEB/PEB izolasyon yamaları yapıldı).
  * `CreateMutexA`, `WaitForSingleObject` ve `ReleaseMutex` ile Multi-Threading senkronizasyonu tamamlandı.
  * `RegCreateKeyExA`, `RegOpenKeyExA`, `RegSetValueExA`, `RegQueryValueExA`, `RegCloseKey` gibi fonksiyonlarla sanal (in-memory) Registry altyapısı kuruldu.
  * Gelişmiş `advanced_win32.exe` test programı tüm bu bileşenleri sıfır hata ile tamamladı.
* **Faz 11: Dosya Sistemi Sanallaştırma (VFS) ve Directory API (Güncel Başarı):**
  * WoW64 mantığına uygun şekilde `C:\Windows\System32` gibi Windows dosya yollarını macOS içerisindeki sanal bir klasöre (`~/.macwi/drive_c/`) yönlendiren POSIX VFS (`macwi_vfs_dos_to_unix`) geliştirildi.
  * `CreateFileA`, `ReadFile`, `WriteFile`, `DeleteFileA`, `GetFileAttributesA`, `SetFileAttributesA` gibi temel dosya operasyonları VFS'e entegre edildi.
  * `FindFirstFileA`, `FindNextFileA` ve `FindClose` API'leri POSIX `opendir`/`readdir` fonksiyonlarıyla bağlanarak dizin döngüleri ve iterasyon desteklendi.
  * `fs_win32.exe` programıyla tüm VFS operasyonları baştan sona test edildi ve hatasız tamamlandı. (Ek olarak FEXCore JIT hafıza çakışmaları çözülerek Image Base ayarları stabiliteye kavuşturuldu.)

* **Faz 12-14: Grafik, Windowing (User32) ve Mesaj Döngüsü (Güncel Başarı):**
  * Gerçek bir GUI penceresi açabilmek için macOS Cocoa (`NSWindow`) tabanlı bir arayüz köprüsü oluşturuldu.
  * `CreateWindowExA`, `RegisterClassExA`, `ShowWindow`, `SetWindowTextA` API'leri entegre edildi.
  * `GetMessageA` ve `DispatchMessageA` ile macOS olay döngüsü (NSEvent) dinlendi ve Windows spesifik `WM_LBUTTONDOWN`, `WM_PAINT`, `WM_CLOSE` mesajlarına çevrildi.
  * `BeginPaint` / `EndPaint` altyapısı kurularak `drawRect:` (Cocoa) ve Emülatör (x86) thread'leri arası deadlock yapmayan senkronize çizim mantığı başarıyla uygulandı (`FillRect`, `TextOutA` testleri geçti).
  * `advanced_gui_test.exe` uygulaması ekrana eksiksiz bir pencere çizerek ve üzerine basılan tuşları algılayarak testleri başarıyla geçti.

* **Faz 15 (Gelişmiş GDI, Zamanlayıcılar ve UI Kontrolleri) (Güncel Başarı):**
  * `CreateFontA` ile macOS CoreText altyapısı üzerinden metinler (font adı, boyutu) render edilebilir hale getirildi.
  * `SetTimer` ve `KillTimer` altyapısı uygulanarak `WM_TIMER` asenkron mesajları JIT döngüsüne entegre edildi.
  * `BUTTON`, `STATIC` ve `EDIT` window sınıfları (Common Controls) için yerleşik (builtin) Window Proc yazıldı. Butonlar native arayüzde çizilebilir hale geldi.
  * `advanced_gui_test.exe` üzerinde testler başarıyla çalıştı.

## 5. Sıradaki Hedefler (Gelecek Aşamalar)

Şu anki mimari Windows sisteminin kalbini (Dosya sistemi, Threading, Senkronizasyon, Memory, Registry) ve arayüzünü (GUI Window, Paint, Event Loop, Timers, Buttons) güvenilir şekilde çalıştırmaktadır. Önümüzdeki en büyük hedef gelişmiş arayüz elementleri, bitmapler ve performans optimizasyonlarıdır:

* **Faz 16 (Gelişmiş GDI Çizimleri ve Bitmapler):** Görüntü (Bitmap) kopyalama, Alpha blending ve cihazdan bağımsız DIB (Device Independent Bitmap) desteğinin eklenmesi.

---
*Bu belge, projenin geldiği en güncel noktayı (Faz 15 sonu) ve teknik temelini yansıtmaktadır.*
