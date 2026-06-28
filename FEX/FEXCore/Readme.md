# FEXCore - Hızlı x86 Çekirdek Emülasyon Kütüphanesi

Bu, FEX emülatör projesi için kullanılan temel emülasyon kütüphanesidir.
Bu proje, diğer x86-64 emülasyon kütüphanelerini karşılayabilecek ve aşabilecek hızlı ve işlevsel bir x86-64 emülasyon kütüphanesi sağlamayı amaçlamaktadır.

### Hedefler
* x86-64 emülasyonu için mevcut seçenekleri geride bırakarak ve aşarak mümkün olduğunca hızlı olmak.
  * Yerel koddan %25 - %50 daha düşük performans arzu edilen hedeftir.
  * x86-64'ü host mimarimize verimli bir şekilde tercüme etmek için bir IR (Ara Temsil) kullanın.
  * Hızlı çalışma zamanı performansı sağlamak için katmanlı bir recompiler desteği sunun.
  * İnceleme ve performans analizi için çevrimdışı derleme ve çevrimdışı araç desteği sağlayın.
  * Kanallı (threaded) emülasyonu destekleyin. x86-64'ün güçlü bellek modelini, zayıf bellek modeli mimarilerinde emüle etmek dahil.
* x86-64 talimat alanının önemli bir bölümünü destekleyin.
  * MMX, SSE, SSE2, SSE3, SSSE3 ve SSE4* dahil.
* Yaygın olarak kullanılmayan x86-64 talimatları için geri dönüş (fallback) rutinlerini destekleyin.
  * x87 ve 3DNow! dahil.
* Yalnızca kullanıcı alanı (userspace) emülasyonunu destekleyin.
  * Tüm x86-64 talimatları CPL-3 (kullanıcı alanı) güvenlik katmanı altındaymış gibi çalışır.
* Test amaçlı minimal Linux Sistem Çağrısı (Syscall) emülasyonu.
* Uygulamalara kolay entegrasyonu desteklemek için taşınabilir kütüphane uygulaması.

### Hedef Host Mimarisi
Bu kütüphane için hedef host mimarisi AArch64'tür. Özellikle ARMv8.1 sürümü veya daha yenisi.
CPU IR, AArch64 göz önünde bulundurularak tasarlanmıştır ancak diğer mimarilere de izin vermelidir.
Geliştirme kolaylığı için x86-64 host desteği mevcuttur, ancak bir öncelik değildir.

### İstenmeyen Özellikler
* Çekirdek alanı (Kernel space) emülasyonu
* CPL0-2 emülasyonu
* Real Mode, Protected Mode, Virtual-8086 Mode, System Management Mode
* IRQ'lar
* SVM
* "Döngü Doğru" (Cycle Accurate) emülasyon
