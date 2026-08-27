# Lab sign-off: VS2022 FULL — `ssl.create_default_context()` and OpenSSL RNG (2026-08-27)

**Branch:** `feature/python-3.12.13-vs2022`  
**Image:** FULL `Python312.efi` (`BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY=1`)  
**Prior lab:** [`2026-08-26_VS2022_FULL_ssl_Shell_exit.md`](./2026-08-26_VS2022_FULL_ssl_Shell_exit.md) (`import ssl` + Shell **`exit`**)

---

## Symptom (after 2026-08-26 ssl/Shell fix)

| Test | Result (before this fix) |
|------|---------------------------|
| `import ssl; print('ok')` | OK; **`Shell>`** → **`exit`** → BIOS OK |
| **`ssl.create_default_context()`** | **Hang** inside Python (no return to **`Shell>`**) |
| VS2022 link (intermediate NASM rewrite) | **`LNK2001`**: unresolved **`OPENSSL_ia32_rdseed_bytes`**, **`OPENSSL_ia32_rdrand_bytes`** from **`rand_lib.obj`** |

**GCC FULL** with the same INF sources did not hang on **`create_default_context()`**.

---

## Root cause

1. **NASM calling convention (VS2022 only):** EDK assembles **`rand_rdrand.nasm`** as **win64** for MSVC. The old assembly read **`rdi`/`rsi`** (System V) while the linker passes **`rcx`/`rdx`**. The byte loop used a garbage length → effectively infinite loop when OpenSSL **`rand_lib.c`** called **`OPENSSL_ia32_rdseed_bytes`** / **`OPENSSL_ia32_rdrand_bytes`** during **`SSL_CTX_new`** (first **`SSLContext()`**).
2. **Missing exports (link):** A broken macro layout did not emit **`global`** symbols with the exact names **`rand_lib.c`** expects → **`LNK2001`** until **`DEF_CPU_RANDOM`** exported **`OPENSSL_ia32_rdseed_bytes`** and **`OPENSSL_ia32_rdrand_bytes`**.
3. **Firmware entropy path:** **`rand_efi.c`** now fills the RAND pool via **`uefi_urandom`** (EFI RNG) with correct **`rand_pool_add_end`** entropy bits instead of relying on a mis-ABI CPU hook from C.
4. **Secondary (context ctor):** UEFI OpenSSL 1.1.1f can misbehave on Python 3.12’s default **`@SECLEVEL=2:…`** cipher string — **`_ssl.c`** uses **`HIGH:!aNULL:!eNULL:!MD5`** under **`UEFI_C_SOURCE`** (3.6.8 AppPkg parity). **`Lib/ssl/_uefi_min.py`** avoids redundant **`verify_mode`** / **`check_hostname`** after the C ctor sets them.

**Not the issue:** GCC-style Shell teardown, monolithic **`ssl.py`**, or “ssl broken on UEFI” in general — see deviations **§11.6–§11.7**.

---

## Fix summary (PyMod)

| File | Change |
|------|--------|
| **`Modules/openssl/efi/src/rand_rdrand.nasm`** | **`DEF_CPU_RANDOM`**: **win64** (`rcx`/`rdx`) vs **elf64** (`rdi`/`rsi`); export **`OPENSSL_ia32_*_bytes`** |
| **`Modules/openssl/efi/src/rand_efi.c`** | Pool fill via **`uefi_urandom`** + proper entropy accounting |
| **`Modules/_ssl.c`** | UEFI cipher list without **`@SECLEVEL=2`** |
| **`Lib/ssl/_uefi_min.py`** | Trust C **`SSLContext`** defaults for client path |

---

## Verification (hardware, 2026-08-27)

```text
Python312.efi -S -c "import ssl; ssl.create_default_context(); print('ok')"
```

- Prints **`ok`**, returns to **`Shell>`**
- **`exit`** → BIOS/setup — **no hang**

---

## Regression commands

| Toolchain | One-liner |
|-----------|-----------|
| **VS2022 FULL** | `import ssl; ssl.create_default_context(); print('ok')` then Shell **`exit`** |
| **GCC FULL** (parity) | Same one-liner after any **`rand_*`** edit |

After **`rand_rdrand.nasm`** changes, confirm **`nasm -f win64`** assembles and link has no **`OPENSSL_ia32_*`** unresolved externals.
