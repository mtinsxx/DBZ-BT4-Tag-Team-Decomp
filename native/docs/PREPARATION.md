# Preparação nativa de recursos — contrato v1

## Código antecipado e dados privados

O único código de execução é C/C++ ARM64 compilado pelo NDK no build do APK.
Não há compilador no app, recompilação do código PS2, transformação LLVM em
runtime, execução guest ou interpretador VU1. Shaders SPIR-V são compilados e
validados no CI; SDL3 cria os pipelines Vulkan a partir deles em cada processo.
Nenhum cache de driver é serializado, necessário ou reutilizado entre versões.
Isso prepara o caminho de textura atual, não pipelines 3D ainda inexistentes.

## Correção da leitura BT4 que faltava na versão 0.5

`android/src/bt4_iso_source.c` tratava DATA/PZS4USn.AFS como CRI AFS clássico,
exigindo `AFS\0`, contagem u32 e pares offset/tamanho. Esse formato servia aos
fixtures e a ferramentas BT3 antigas. O extrator BT4 existente em
`port/setup/native.cpp:step_data` já estabelece outro contrato: índices em
BIN/DBZ4.BIN e contêineres de setores cumulativos. A leitura anterior não
implementava esse contrato, embora validasse corretamente os programas.
Não há um ZIP de diagnóstico 0.5 disponível no cloud; não se afirma ter lido
seu errno/magic real. A incompatibilidade de implementação é comprovada pelo
código e foi reproduzida por um ISO sintético sem cabeçalho AFS clássico.

O resolvedor agora encontra o nome `pzs4usN.afs\0` no DBZ4, recua 16 bytes e
valida os quatro campos descritos pelo extrator: zero, tamanho 0x11A+2*count,
count e count. As contagens são 50503/3400/32774/30000; ENG tem 32775 no
contêiner 2. Cada u16 em +0x11A especifica setores de 2048 bytes. Somam-se
extensões, com aritmética de 64 bits e limites do contêiner e da ISO. Somente
o último membro pode perder menos de um setor de padding, como no extrator.

Para o retrato, global 0x71C -> partição 1, membro 1819; ambos os catálogos
`plat_verify_tab{,_eng}.h` registram 63488 bytes e CRC32 f1186a78. Preserva-se
a validação dos três programas ENG/ESP antes de qualquer preparação JNI. O
CRC é calculado sobre os próprios bytes usados pelo conversor, em uma leitura,
sem intervalo entre fingerprint de um conteúdo e conversão de outro.

AFS clássico continua suportado quando não há DBZ4, preservando os testes e
leitores anteriores; a preparação BT4 não usa esse fallback, pois exige DBZ4
compatível. Um índice BT4 presente e malformado falha: não se tenta adivinhar
outros arquivos. `Bt4Iso_LastError()` fornece estágio e locator; sucesso BT4
registra contêiner, índice, contagem, offset ISO e tamanho. O log de preparação
inclui essa origem ao confirmar a conversão.

## Cache e recuperação

Local: filesDir/native-assets-v1/{ENG,ESP}/. Um lock filesDir/native-assets.lock
serializa preparação, leitura do cache e limpeza entre processos. Um mutex JNI
impede que jobs de Activities recriadas troquem o fd ISO global entre tarefas.
O descriptor SAF é duplicado e retido somente durante a operação; nada copia
a ISO para filesDir e a limpeza não altera o grant SAF ou o arquivo externo.

O conjunto implementado é **portrait-feasibility-v1, um recurso**, não o conjunto
obrigatório do jogo. Não existe contagem conhecida/conversão de todos os modelos,
materiais, esqueletos, animações, áudio ou cenários. O manifesto diz explicitamente
`game-ready=false`; alcançar 1/1 não habilita INICIAR JOGO. O teste Vulkan separado
só lê a imagem preparada com integridade válida, sem reconversão no renderer.

Arquivos fixos: portrait.rgba8 (256x512 RGBA8_UNORM, 524288 bytes) e manifest.txt.
O manifesto canônico inclui schema, versão do conversor, catálogo, perfil,
locator, tamanho/CRC32 de origem, SHA-256 da origem, dimensões/formato,
SHA-256 da saída e SHA-256 do próprio corpo do manifesto. Os hashes detectam
corrupção acidental; não constituem uma assinatura contra alteração maliciosa
de todos os arquivos pelo próprio dono do sandbox. Os programas e o CRC/tamanho
originais continuam sujeitos ao catálogo conhecido.

Antes da conversão, `fstatvfs` exige a saída + 4096 bytes de manifesto + 8 MiB
de reserva. O cache atual é limitado a um resultado por perfil e nomes fixos;
não acumula um diretório por tentativa/commit. Não se extrai toda a ISO/AFS.
O orçamento terá de ser atualizado quando conversores reais forem adicionados.

Gravação: temporário exclusivo/no-follow -> write -> fsync -> rename -> fsync
do diretório. O manifesto é publicado por último, seguido de read-back e
verificação de integridade. Diretórios novos também são sincronizados.
Fechamento/process death antes do commit deixa um estado ausente/inválido;
na próxima preparação, temporários não são confiados e o recurso é reconstruído.
Depois de um commit válido, ele é reutilizado sem leitura/reconversão da origem;
a ISO e seu perfil ainda são verificados pela entrada JNI.

Interrupção é cooperativa entre fases (validação/decodificação de um recurso
pequeno não são preemptadas). onDestroy solicita interrupção; se Android matar
o processo, o sistema libera flock/fds. Não se promete execução em segundo plano
permanente. Reabrir e tocar Preparar/retomar recupera os commits válidos.

Progresso reporta recursos confirmados/total deste conjunto, quantidade reutilizada,
bytes de origem processados na tentativa, bytes de origem do catálogo e bytes de
saída confirmados. Reuso informa zero bytes de origem processados e reused=1.
Não há porcentagem, tempo estimado ou contagem fictícia de recursos do jogo.

Limpar remove somente os quatro nomes de dados/temporários conhecidos nos dois
perfis, mantendo o lock e qualquer outro arquivo. Reconstruir limpa esse conjunto
e o prepara outra vez. Symlinks de arquivos/diretórios não são seguidos.

## Investigar os próximos recursos reais

A ação opcional de investigação lê o modelo character0/costume0/intacto e as
duas tabelas associadas diretamente da ISO, sem extrair/copiar os blobs:

| Locator | Bytes | CRC32 | Evidência |
| --- | ---: | --- | --- |
| pzs4us2/02250.bin | 759808 | 04a30b50 | Bt4_ModelFile, btl_obj, catálogos ENG/ESP |
| pzs4us2/01750.bin | 1058816 | d7db4ba9 | BtlRes_Request, catálogos ENG/ESP |
| pzs4us2/02000.bin | 718848 | 52f63b48 | BtlRes_Request, catálogos ENG/ESP |

`BObjFile_GetEntry` em battle/bobj_a.c estabelece offsets em bytes com dois bits
baixos descartados; entradas iguais são vazias. BtlObj_BindTables usa entrada 3
para o modelo e 12 para texturas. O inspector copia a semântica de acesso com
bounds, sem converter offsets em ponteiros PS2 nem modificar o blob.

`describeModel` investiga meshOfs +0x6C, cabeçalhos relativos next/last/node e
início do stream +0x60, conforme gfxm_e.h/gfxm_e_b.c; TexFile/TexEntry têm tabela
em palavras e pixels/CLUT em palavras, conforme gfxm_b_b.h/Res_RelocateOffsets.
O primeiro movimento em file[1] é expandido por BPE e reporta duração/eventCount,
conforme BtlObjAnim_Load/bobj_a.h. File[2] tem outras tabelas: nenhum codec é
atribuído por suposição. Falha de interpretação é reportada, não aceita como
uma malha/pose válida. O diagnóstico inclui hashes, contagens e offsets, nunca
os arquivos, coordenadas, texturas ou bytes completos comerciais.

**Isso é inspeção de metadados, não conversão 3D.** Pesos/bind pose, atributos e
índices do stream, semântica de materiais, channels/eventos completos e comparação
visual/numérica com referência continuam bloqueando a reconstrução. O cloud não
tem esses três membros nem a ISO. O próximo teste no Galaxy pode fornecer o
relatório sem pedir ao proprietário que execute comandos ou extraia arquivos.

## Validação e limites

Testes novos com ASan/UBSan: contêiner BT4 sem AFS clássico, tabela inválida ou
truncada, índice fora da contagem, extents e último padding; vetores SHA-256
FIPS (vazio, abc, fronteira de bloco e um milhão de 'a'); read-back, reuso sem ISO,
perfis separados, corrupção, cancelamento, processo filho encerrado com _exit
entre commit de dados/manifesto, retomada, lock, limpeza e temporário symlink;
parsing de offsets/meshes/texturas apenas com fixtures sintéticos.

Os 18 testes herdados passam após a alteração no leitor. O alvo novo tem link
ARM64 --no-undefined. Não há execução Android/driver/SAF físico no cloud nem
conversão comprovada de bytes comerciais nesta sessão. Somente o proprietário
pode confirmar a conversão real e o quadro no Galaxy S24 FE.
