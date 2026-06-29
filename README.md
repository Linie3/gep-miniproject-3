## CLion Setup

Compiler:
Build, execution... -> Toohchains -> VS

Custom Build Target:
Build:
Program: scons
Arguments: dev_build=yes dev_mode=yes target=editor debug_symbols=yes
Working directory: $ProjectFileDir$ 
Clean:
Program: scons
Arguments: -c
Working directory: $ProjectFileDir$

Run Config:
erstelltes build tool auswählen
kompilierte binaRy auswählen
--editor --path C:\Users\lnia2\projects\gep-miniproject-3-game
Run as administrator

scons compiledb=yes compile_commands.json to update intellisense

## Libply

Thirdparty lib to read ply headers. Add following to `thirdparty/README` and then put extracted files into `thirdparty/libply/:

```markdown
- Upstream: https://github.com/nmwsharp/happly/
- Version: 1.1.0 (ed5480454d15e5287370eadc3682e9d40ba395e1, 2026)
- License: MIT

Files extracted from upstream source:

- `happly.h`
- `LICENSE`
```