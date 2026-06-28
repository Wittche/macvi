# FEX-macOS: RootFS-Less & Linux-Less Mimari İlerleme Raporu

## 🚀 Özet: İlerleme Var mı?
**Kesinlikle EVET! Projeyi tam fonksiyonel çalışan, canlı etkileşimli bir sisteme dönüştürdük.** x86_64 guest JIT kodu, host üzerindeki `RegistryEmulator` ile tamamen entegre çalışmakta ve Cocoa GUI penceremizde yapılan seçimler (örneğin Windows sürümü) anlık olarak bu sanal registry veritabanına yazılmaktadır.

Aşağıdaki kritik aşamalar başarıyla tamamlandı:
1. **Tam Arınma:** Linux emülasyonu, GDB server, FEXServer ve binfmt_misc gibi tüm hantal yapılar silindi. FEX artık saf bir macOS kütüphanesi.
2. **PELoader Mükemmelleştirildi:** `winecfg.exe` gibi karmaşık PE dosyaları artık segment bazlı olarak belleğe kusursuz haritalanıyor ve tüm DLL importları (IAT) başarıyla çözülüyor.
3. **Dinamik Thunk Takibi (Tracing):** `PELoader` içinde çözülen tüm IAT thunk'ları host tarafında bir global tabloya kaydedilerek `HandleSyscall` anında tam sembol adıyla konsola yazdırılmaktadır.
4. **Wine Hata İzleme (Debug Interception):** `__wine_dbg_header` ve `__wine_dbg_output` fonksiyonları yakalanarak Wine log kanallarının ürettiği tüm çıktılar host terminaline aktarılmaktadır.
5. **Cocoa / AppKit Pencere Entegrasyonu:** `PropertySheetW` çağrısı yakalandığı anda ana thread üzerinde Cocoa `NSWindow` ve `NSTabView` tabanlı (Applications, Libraries, Graphics, About vb. içeren) **yerel macOS arayüzü oluşturuldu ve ekrana getirildi.**
6. **Canlı Kayıt Defteri Entegrasyonu (Bidirectional Registry):** 
   - `RegOpenKeyExW` (ID 80), `RegQueryValueExW` (ID 81) ve `RegCreateKeyExW` (ID 82) guest thunk'ları tamamen host tarafındaki `RegistryEmulator` sınıfına bağlandı.
   - ASCII depolanan kayıt değerleri guest tarafı için UTF-16 / Unicode standardına dinamik olarak dönüştürülüp beslendi.
   - Cocoa penceresinde kullanıcı Windows sürümünü değiştirdiğinde (`Windows 11`, `Windows 7`, `Windows 10`), bu seçim anlık olarak kayıt defterine (`ProductName`, `CurrentBuild` anahtarlarına) yazıldı.
   - Pencere açılırken kayıt defterindeki mevcut değer okunarak Cocoa PopUp selection'ı otomatik olarak o değere konumlandırıldı.

---

## 🛠️ Yapılan Teknik İyileştirmeler

### 1. Dinamik Linker Çözümleri
- `FEXCore_shared` ve `MacOSEmulation` arasındaki dairesel bağımlılık, registry çağrıları için de `dlsym(RTLD_DEFAULT, "...")` dinamik yükleyicisi ile aşılmıştır.
- Yeni eklenen `FEX_Registry` C-API fonksiyonları `Main.mm` içerisindeki volatile tablosuna eklenerek Xcode derleyicisinin bunları optimize edip silmesi (stripping) engellenmiştir.

### 2. Unicode Dönüşüm Güvenliği
- `RegQueryValueExW` thunk'ı, registry içindeki verileri okuyup guest belleğe yazarken string tiplerini (`REG_SZ`) otomatik olarak UTF-16 formatına çevirerek guest uygulamanın Unicode karakterlerde çökmesini engellemiştir.

---

## 🚧 Mevcut Durum: TAM FONKSİYONEL BAŞARI!
Winecfg uygulaması başarıyla arayüzünü açmakta, kayıt defteri değerlerini okumakta ve arayüzdeki değişiklikleri kayıt defterine geri yazmaktadır. JIT motorumuz ve Cocoa host köprümüz sorunsuz ve stabil şekilde entegre edilmiştir.

---

## 📈 Sonraki Odak Noktaları
1. Gelişmiş UI tablarının (Drives, Audio vb.) host sistemin disk/ses servislerine bağlanması.
2. Metal grafik motorunu kullanan 3D Windows oyunları için thunk kütüphanelerinin genişletilmesi.
