# FEX-Emu Kaynak Kodu Anahattı

## FEXCore
Daha fazla ayrıntı için [FEXCore/Readme.md](../FEXCore/Readme.md) belgesine bakın.

### Sözlük

- **Splatter:** Isel yapmak yerine yapılandırılabilir makroları birleştiren bir kod oluşturucu arka ucu.
- **IR (Intermediate Representation):** Ara Temsil; Arm64'ü gevşek bir şekilde modelleyen yüksek seviyeli işlem kodu temsilimiz.
- **SSA (Single Static Assignment):** IR'yi bellekte temsil etme biçimi.
- **Basic Block:** Kontrol akışı olmayan, kontrol akışı ile sonlanan bir talimat bloğu.
- **Fragment:** Temel blokların bir koleksiyonu; muhtemelen tüm bir guest fonksiyonu veya onun bir alt kümesi.

### backend (Arka Uç)
IR'den host kod üretimi.

#### arm64
- [ALUOps.cpp](../FEXCore/Source/Interface/Core/JIT/ALUOps.cpp)
- [Arm64Relocations.cpp](../FEXCore/Source/Interface/Core/JIT/Arm64Relocations.cpp): Arm64 splatter arka ucunun yer değiştirme (relocation) mantığı.
- [AtomicOps.cpp](../FEXCore/Source/Interface/Core/JIT/AtomicOps.cpp)
- [BranchOps.cpp](../FEXCore/Source/Interface/Core/JIT/BranchOps.cpp)
- [ConversionOps.cpp](../FEXCore/Source/Interface/Core/JIT/ConversionOps.cpp)
- [EncryptionOps.cpp](../FEXCore/Source/Interface/Core/JIT/EncryptionOps.cpp)
- [JIT.cpp](../FEXCore/Source/Interface/Core/JIT/JIT.cpp): Arm64 splatter arka ucunun ana bağlantı mantığı.
- [JITClass.h](../FEXCore/Source/Interface/Core/JIT/JITClass.h)
- [MemoryOps.cpp](../FEXCore/Source/Interface/Core/JIT/MemoryOps.cpp)
- [MiscOps.cpp](../FEXCore/Source/Interface/Core/JIT/MiscOps.cpp)
- [MoveOps.cpp](../FEXCore/Source/Interface/Core/JIT/MoveOps.cpp)
- [VectorOps.cpp](../FEXCore/Source/Interface/Core/JIT/VectorOps.cpp)

#### shared
- [CPUBackend.h](../FEXCore/Source/Interface/Core/CPUBackend.h)

### frontend (Ön Uç)

#### x86-meta-blocks
- [Frontend.cpp](../FEXCore/Source/Interface/Core/Frontend.cpp): Talimat ve blok meta bilgilerini çıkarır, ön uç çoklu blok mantığını yönetir.

#### x86-tables
- [BaseTables.cpp](../FEXCore/Source/Interface/Core/X86Tables/BaseTables.cpp)
- [X86Tables.h](../FEXCore/Source/Interface/Core/X86Tables/X86Tables.h)

### glue (Bağlantı Mantığı)
Çeşitli parçaları birbirine bağlayan mantık.

#### driver
Emülasyon ana döngüsü ile ilgili bağlantı mantığı.
- [Core.cpp](../FEXCore/Source/Interface/Core/Core.cpp): Frontend, OpDispatcher, IR Opts & Compilation, LookupCache ve Dispatcher'ı birbirine bağlar ve Yürütme döngüsü giriş noktasını sağlar.

### ir
- [IRDumper.cpp](../FEXCore/Source/Interface/IR/IRDumper.cpp): IR'yi metne dönüştürür.
- [IREmitter.cpp](../FEXCore/Source/Interface/IR/IREmitter.cpp): IR oluşturmak için C++ fonksiyonları.

## ThunkLibs
Daha fazla ayrıntı için [ThunkLibs/README.md](../ThunkLibs/README.md) belgesine bakın.
Bu kütüphaneler, guest çağrılarını host kütüphanelerine (OpenGL, Vulkan, SDL2 vb.) yönlendiren aracı kütüphanelerdir.

## unittests (Birim Testleri)
Daha fazla ayrıntı için [unittests/Readme.md](../unittests/Readme.md) belgesine bakın.
