# mengetik-raw

Mesin pembaca file RAW kamera untuk **Raw Edit** di [mengetik.id](https://www.mengetik.id/p/raw-edit.html).

- `libraw-mengetik.wasm` : LibRaw 0.22.1 yang dikompilasi ke WebAssembly (wasm32-wasi, tanpa thread),
  dipanggil oleh halaman Raw Edit lewat jsDelivr:
  `https://cdn.jsdelivr.net/gh/<username>/mengetik-raw@main/libraw-mengetik.wasm`
- `source/` : kode pembungkus (`wrap.cpp`) dan skrip build (`build.sh`, memakai Zig).
  Kode sumber LibRaw: https://www.libraw.org/download

## Lisensi
LibRaw dirilis dengan lisensi ganda LGPL 2.1 / CDDL 1.0 (lihat `LICENSE.LGPL`, `LICENSE.CDDL`, `COPYRIGHT`).
File .wasm ini bisa dibangun ulang dari sumber LibRaw + `source/wrap.cpp` dengan `source/build.sh`.
