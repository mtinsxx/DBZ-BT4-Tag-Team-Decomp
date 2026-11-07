# Engine nativa e preparação de recursos

O alvo C++ ARM64 é compilado antecipadamente no APK, sem compilar ou executar
código PS2 no Android. A versão 0.6 prepara o retrato original em cache privado,
com manifesto versionado/hashes, e o renderer Vulkan consome esse cache.
O jogo completo ainda não está implementado; INICIAR JOGO permanece bloqueado.

Consulte [PREPARATION.md](docs/PREPARATION.md) para o índice DBZ4, recuperação,
progresso real, testes e investigação dos próximos modelos/animações.

## Evidência do recurso escolhido

- `port/src/fight_panels.c`: FACE_FILE = 0x71C e partição 1, índice id-1.
- `include/menu/menu_d.h`, `src/menu/menu_h_b.c`: retrato do personagem 0.
- `port/src/gs/bt4_eye_anchors.h`: CRC32 compactado **f1186a78** para esse retrato.
- `port/src/gs/fight_panels_ui.cpp`: BPE, 256×512 índices de 8 bits,
  CLUT de 256 cores, troca dos bits 3/4 da paleta, alpha PS2 convertido.
- `src/sys/bpe.c` e `include/sys/gfxm_b_b.h`: compressão e campos de offsets.

Logo, a leitura concreta é `DATA/PZS4US1.AFS`, membro **1819** (0x71B),
endereço lógico `pzs4us1/01819.bin`. Não usamos o antigo 0x2F8 de documentação
BT3 para selecionar dados BT4. Os três programas originais da ISO continuam
sendo verificados pelo catálogo ENG/ESP existente; o retrato também precisa
coincidir com o CRC conhecido. Não existe busca de uma imagem qualquer como
fallback, textura embarcada ou fixture no APK.

O conversor é derivado do leitor existente, com limites adicionais: entradas
truncadas, expansão incompleta/excessiva, ciclos e offsets inválidos falham.
A disposição dos pixels é convertida por uma permutação de índices de 64 KiB;
nenhuma VRAM guest, registro GS, pacote DMA ou instrução VU1 é executado. O
resultado é `Image {width,height,source_crc,vector<uint8_t> rgba}` com ownership
nativo. Campos de 32 bits existem apenas como formato no disco.

## O que o teste visual demonstra

O shader desenha uma superfície de apresentação com a textura original e
preserva sua proporção/alpha. Essa superfície **não é geometria de personagem**.
Modelos 3D, esqueletos, animações, materiais 3D, combate e menus do jogo não
estão implementados neste alvo. O teste não serve para declarar BT4 funcional.

A preparação registra VERIFYING/CONVERTED/RESOURCE_READY_GAME_INCOMPLETE.
O renderer registra ENGINE_READY, CACHE_VALID, VULKAN, SUBMITTED ou
ERROR em `native-engine-status.txt` e no log. Uma primeira submissão bem-sucedida
aguarda fence e tenta capturar o alvo GPU próprio em `native-first-frame.png`.
Readback/submissão não comprovam visualização física; só o proprietário valida
isso no Galaxy. Nenhum primeiro quadro real foi obtido no cloud.

A Activity SDL usa o processo `:native_engine`; o launcher continua em outro
processo. Isso evita que a saída do SDL finalize também a tela de diagnóstico.
O launcher apresenta ERROR ou o estágio de uma interrupção ao retornar; o ZIP
inclui status, logs, BUILD.json, captura quando existente e registros/traces
ApplicationExitInfo do aplicativo quando Android os fornece. Não inclui ISO,
AFS, programa ou dump de recursos comerciais. Erros não viram uma tela pronta.

## Build e validação

Host, sem LLVM, SDL ou NDK:

```sh
cmake -S engine/native -B build/native-host -DCMAKE_BUILD_TYPE=Debug -DNATIVE_SANITIZERS=ON
cmake --build build/native-host --parallel 2
ctest --test-dir build/native-host --output-on-failure
```

Os fixtures sintéticos exercitam BPE válido, truncamentos, ciclos, limites,
vetores independentes de disposição/paleta/alpha e leitura ISO/AFS real por fd
retido, incluindo CRC incorreto e recurso ausente. Não são recursos BT4 e
não são usados pelo runtime Android. LeakSanitizer fica desativado pela
restrição ptrace do executor; ASan/UBSan permanecem ativos.

ARM64 usa NDK r28c (`28.2.13676358`), API 28, SDL 3.4.12 e SPIR-V Vulkan 1.0,
compilados diretamente por CMake/Clang normal, sem ptr32/transformação LLVM.
Veja `.github/workflows/native-engine.yml` no repositório principal: ele busca
somente SDL pelo hash já fixado, compila shaders offline e linka o alvo com
`--no-undefined`. O workflow antigo de link integral não é disparado automaticamente nessa branch.

O empacotador `tools/cloud/build-native-apk.py` verifica AArch64 ELF64,
alinhamento LOAD de 16 KiB dos dois .so, ausência de símbolos do motor antigo,
dependências, assinatura e alinhamento ZIP. Também rejeita símbolos Sony
`sce*` no link (wrappers não usados do leitor compartilhado são ocultos e
eliminados pelo linker). Produz
`build/native-apk/TenkaichiMobile.apk` apenas com SDL e o novo alvo. O CI publica
`TenkaichiMobile-native-preparation-arm64`, sem release.

## Limites e próximo passo

Só o main ELF privado está disponível no cloud; não temos o membro 1819 da AFS,
ISO completa ou Galaxy. O algoritmo tem evidência no código anterior e testes
sintéticos; o decode desse arquivo comercial e a apresentação no Xclipse ainda
precisam da execução no aparelho. Se seu CRC/layout divergir, o teste deve parar
e registrar os dados observados, sem converter por adivinhação.

A próxima etapa depende desse resultado: confirmar o recurso e então converter
um modelo real com mesh, esqueleto e animação comprovados. Consulte
[viabilidade e migração](docs/ARCHITECTURE.md) para formatos, candidatos concretos
e plano de preservação da jogabilidade.
