# Auditoria do boot nativo Android — execução antes de funcionalidades

## Conclusão e evidência disponível

A arquitetura contém o motor C real, menus, batalha, GS/VU1 e backend Vulkan.
Ela **não tinha uma integração de boot suficientemente correta para afirmar
que executaria no Android**. O linker e o APK não demonstram execução. Esta
revisão corrige três incompatibilidades concretas, mantém o importador SAF e
prepara evidência no caminho real. Não há primeiro quadro observado no Galaxy.

O diagnóstico fornecido pelo proprietário corresponde a `a49f919` / `f727d82`:
ENG, main 2115696 bytes / CRC32 9d2224c1, três programas compatíveis e leitura
SAF sem cópia. A execução para em `Guest memory mapping failed`. Os registros
Android dizem EXIT_SELF (1), status 0, sem trace; não provam um sinal nativo.
O log antigo não tem endereço/errno. Nenhum errno específico é inventado.
Não recebemos diagnóstico do APK 0.3 após a mudança de reserva antecipada.

## Falhas estruturais corrigidas

### 1. O carregador Android não reserva a janela vizinha à biblioteca

`bt4_window_runtime.c` fixava W = imagem - 512 MiB. A revisão 0.3 antecipou
as reservas, mas ainda podia colidir com VMAs que já existiam antes de dlopen.
ASLR do ELF não promete espaço nas seis áreas calculadas a partir dessa base.

A auditoria do pass mostra que código, callbacks e inicializadores ELF já usam
`Port_Window` em runtime. A exceção era `-bt3-abs=D_3BE71C`, ainda materializado
como `__ehdr_start - 512 MiB + endereço`. Isso impedia escolher outra base.
Agora esse símbolo permanece em address space 271 até a materialização runtime.
Uma tentativa de usar a fórmula constante no modo ELF falha no compilador.
O modo Mach-O preserva sua fórmula original.

Antes dos construtores de ponteiros, `Bt4Memory_Select` tenta a posição antiga
e posições alternativas, com distâncias imagem/W de 512 a 1536 MiB, em passos
de 16 MiB. Cada candidato precisa reservar **todas** as seis áreas com
NOREPLACE, sem substituir VMAs. Somente EEXIST justifica outro candidato;
EPERM, ENOMEM e falha de cleanup interrompem a seleção. A imagem ELF inteira
fica dentro da janela com offsets positivos menores que 2 GiB, preservando
inclusive o bit reservado a scratchpad nos pacotes DMA. O layout PS2 não muda.

W é publicado uma única vez, antes dos construtores 102/103/104. Nunca muda
quando ponteiros já existem. Se nenhum candidato estiver disponível, a última
base válida permite apenas os construtores de metadados; o commit explícito
continua bloqueando o jogo. Não há execução parcial para contornar falhas.
A permissão RW continua limitada às reservas próprias; rollback preserva VMAs
alheias. Sem MAP_FIXED, RWX, desativação de ASLR ou truncamento silencioso.

### 2. Selecionar Vulkan não ativava o GS

`gPortDataStripped` permanece 2 nesta integração. `gs_mode()` escolhe GPU por
padrão somente se essa flag for 1; caso contrário, sem BT3_GS, desliga o GS.
O APK configurava apenas BT3_GPU_API=vulkan. Assim, ultrapassar a memória não
garantia sequer chamar `GsGpu_Init`.

O boot agora configura explicitamente **BT3_GS=gpu**, Vulkan estrito e render
na thread do jogo. Não altera a flag desktop, que também controla o carregador
.dat. No Android, falha de inicialização GPU encerra com erro explícito, em vez
de continuar silenciosamente no rasterizador de referência ou sem imagem.
Falhas de aquisição/submissão também são registradas. Swapchain NULL durante
minimização continua sendo um estado de ciclo de vida válido.

### 3. Mudar SP não registra uma stack no Bionic/ART

O trampoline AAPCS64 mudava SP dentro da thread Java de SDLActivity já anexada
à VM. Preservar x18 e registradores não atualiza os limites de stack conhecidos
por pthread/ART. As chamadas seguintes de vídeo/áudio do SDL usam JNI.
Não há evidência de que esse caminho tenha chegado a executar no aparelho;
é uma incompatibilidade de contrato identificada no código, não um crash
observado inventado.

Agora `Bt4Thread_Run` cria uma thread nativa joinable com
`pthread_attr_setstack` sobre os 16 MiB já comprometidos. Verifica os erros
pthread sem confundir seu retorno com errno; não retorna com contexto/stack
vivos após falha de join. A thread valida os limites efetivos por
pthread_getattr_np, alinhamento e codificação do frame; aceita a redução útil
que o Bionic faz ao reservar pthread_internal_t no topo (sem exigir 16 MiB
úteis idênticos aos solicitados). Registra o topo abaixo
dos metadados Bionic para snapshots futuros. Depois se anexa pela API pública
SDL_GetAndroidJNIEnv e inicializa SDL VIDEO nessa mesma thread, que o SDL define
como thread de vídeo no Android. Só então chama o verdadeiro Game_Main.
A thread Java original aguarda join; sua stack e JNIEnv não são transplantados.
O código SDL fornecedor não foi alterado.

## Integração auditada, sem inferir execução de uma referência de linker

| Camada | Caminho efetivo | Limite da evidência |
| --- | --- | --- |
| Dados | SAF fd → pread limitado → ISO9660/AFS/DVD; três CRCs; 2388 spans de dados, ponteiros preservados | ENG/SAF passaram no aparelho; importação de tabelas ainda não observada ali |
| ARM64/LLVM | 274 unidades C originais; ptr32, ABI, callbacks, soft-float do jogo e float nativo do renderer | Testes executam casos reais no host e emitem ARM64; não validam todas as cenas |
| Memória | seleção antes de ponteiros → reservas próprias → mprotect RW → alocador | Testes de mmap reais e construtor de produção no Linux; falta confirmar Android |
| Entrada | SDL_main → thread registrada/JNI → Game_Main original | Checkpoint agora está dentro de Game_Main; o antigo log era anterior à chamada |
| Processamento | inicializadores → Common_LoadBoot → Progress_Main real → Battle_Main real | Marcos não simulam avanço; bloqueios posteriores exigem dados completos/aparelho |
| DMA/GS | Dma_Flush → D1_CHCR/Port_DmaChcr → Port_GsVif1Chain/GIF → Gs_Gif/GsVu1_Call | Hooks PORT existem também no IR; não basta uma biblioteca GS estar ligada |
| GPU | GsGpu_Init → SDL Vulkan device/window → listas/draws → swapchain submit | Erros param; não existe apresentação Vulkan comprovada no Galaxy ainda |

`portsrc.prepare` encontra C para as 44 funções referidas por INCLUDE_ASM nos
274 arquivos usados. Ele habilita tentativas C não matching. Isso evita
executar MIPS no aparelho, mas **não transforma essas tentativas em prova de
equivalência**. A fidelidade precisa de testes de estado/execução e comparação
com evidência original, sobretudo ao aparecer uma divergência concreta.

## Substituições PS2 que não devem ser confundidas com motor completo

A camada desktop contém retornos vazios para setup de IOP/kernel e carregamento
de IRX, pois não existe IOP no port. Serviços usados pelo jogo são substituídos
por arquivos, áudio, pad, memória e desenho nativos. Isso precisa ser auditado
por contrato, não por quantidade de símbolos resolvidos. Nenhum novo stub foi
adicionado nesta revisão.

Em particular, sceOpen/Lseek/Read do headless.c tornam Overlay_Load uma operação
sem cópia. Isso é intencional no port estático: o código C do menu já está ligado
e seus dados são importados de DBZP. Copiar MIPS sobre uma função ARM64 seria
errado. Sem BT3_DEMO/BT3_REPLAY, __wrap_Progress_Main chama __real_Progress_Main;
não há batalha artificial selecionada pelo APK.

Ainda existem limites reais: MPEG suporta somente I-pictures; state.c procura
um .mem de executável desktop e não fornece globals completos no Android
(rollback/netplay pendentes, sem impedir o boot offline); a interface touch
moderna e o ciclo completo de pausa/retomada ainda requerem trabalho. GS/VU1
mantém limites de cobertura e registra instruções não implementadas; preservá-lo
não é garantia de renderização perfeita. Esses limites não foram ocultados.

## Evidência produzida pela próxima execução real

- mmap/mprotect: candidato, W, imagem/tamanho/offset, áreas, flags, errno,
  primeiro/último erro e VMAs sobrepostas.
- thread: limites reais da stack, frame/offset, attachment JNI e thread de vídeo.
- checkpoint dentro de Game_Main e antes de cada inicializador, primeiras
  esperas VBlank, primeira cadeia VIF1 e chamada VU1.
- GPU: device/backend, janela, erros de aquisição/submissão.
- após submissão de swapchain e draws GS reais no alvo, readback com fence;
  somente RGB não uniforme pode gerar `tenkaichi-first-gs-frame.png`.

A captura lê o alvo GS de 512×448; não gera geometria, não desenha uma tela
substituta e não entra no jogo como textura. Clear/UI desktop não são evidência.
Até 64 readbacks de diagnóstico, uma imagem privada, apagada no próximo boot e
incluída apenas no ZIP exportado pelo proprietário. Readback + submit não é
confirmação física de display, gameplay ou desempenho. O proprietário precisa
verificar o resultado visual. Nenhuma captura foi produzida no cloud.

## Validação e bloqueio restante

18 casos host (incluindo LLVM, seleção com colisão, construtor real e pthread
com stack registrada), 11 grupos ASan/UBSan e 81 regressões desktop foram
exercitados. Os fixtures não contêm dados comerciais e seus resultados não
são apresentados como execução do jogo. O build integral e o Actions são
registrados separadamente na PR; um link verde não conclui este marco.

O cloud tem apenas o main ELF privado, sem ISO/AFS/DBZP completos e sem o Galaxy.
Não é possível produzir aqui uma execução real de Game_Main com seus recursos,
nem um primeiro quadro Xclipse. O próximo bloqueio verificável depende da
execução desta correção no Galaxy e de seu ZIP: a fase exata indicará se o
problema passou a ser attachment/vídeo, inicialização de algum subsistema,
leitura de recurso ou comandos/renderização. Não é necessário que o
proprietário execute adb ou interprete logs.

Referências de contrato consultadas: SDL 3.4.12 `src/core/android/SDL_android.c`
(Android_JNI_GetEnv/AttachCurrentThread), `src/SDL.c` (SDL_VideoThreadID) e
[AOSP Bionic pthread_create.cpp](https://android.googlesource.com/platform/bionic/+/refs/heads/main/libc/bionic/pthread_create.cpp)
(`__allocate_thread`, redução de stack_size por pthread_internal_t). As fontes
explicam o contrato; não substituem a validação da versão Samsung no aparelho.
