# Viabilidade, auditoria e migração nativa ARM64

## Decisão e preservação

A decisão do proprietário é reconstruir os sistemas nativos preservando
comportamento verificável, sem depender permanentemente da memória/serviços
internos PS2. Os commits `e46f12b` (motor) / `d3df5f9` (app), branches, testes,
licenças e documentos antigos ficam preservados. Não houve merge na main.
O novo alvo tem seu próprio grafo de build, app, tipos e testes; não modifica
nem remove o runtime antigo. ISO9660/AFS e fingerprints são reutilizados como
fontes C independentes, com os mesmos contratos de descriptor e limites.

## Por que a abordagem anterior parou

O diagnóstico 0.4 informado pelo proprietário (Android 17, Galaxy S24 FE) mostra
65 candidatos, falha no heap PROT_NONE/NOREPLACE, EEXIST e VMAs ART/Dalvik
LinearAlloc/shadow. Isso comprova que a busca implementada não encontrou as
seis áreas; não comprova que toda arquitetura compacta imaginável seja inviável.

A abordagem exigia que código/dados ELF e regiões guest coubessem na mesma
janela e que offsets fossem representáveis em 32 bits (com restrições adicionais
para preservar bits DMA). Escolher W em relação ao ELF já carregado não controla
as VMAs que o ART criou antes do dlopen. Reservar primeiro e carregar o ELF numa
região própria seria outra arquitetura de loader, com muitas obrigações de ABI
e ownership; aumentar deslocamentos não a substitui. Ela não foi implementada
nem declarada validada. A nova decisão evita esse contrato inteiro: malloc,
vector, objetos e código usam ponteiros nativos e ASLR normal. Não há
MAP_FIXED, MAP_FIXED_NOREPLACE ou reserva de uma janela PS2 neste alvo.

| Aspecto | Linha compacta preservada | Novo alvo |
| --- | --- | --- |
| Dados em execução | layout PS2/ponteiros compactos adaptados por LLVM | tipos próprios, ownership RAII, índices/handles |
| Code/data placement | alcance comum limitado | endereços nativos independentes |
| Serviços | adaptadores Sony/CRI/IOP | SDL/Android e serviços C++ |
| Desenho | pacotes GS/VIF/VU1 e shaders do port desktop | recursos convertidos → materiais/meshes nativos → Vulkan |
| Reuso imediato | grande parte do C decompilado | leitores seguros, conhecimento de formatos e algoritmos isolados |
| Risco principal | ABI/layout/endereço e serviços legados | fidelidade de gameplay e conversão dos recursos |

Não se promete equivalência automática: remover a dependência de layout torna
necessário converter semântica e dados, testar e comparar com o original.

## Auditoria de reuso

| Componente | Decisão | Evidência/trabalho necessário |
| --- | --- | --- |
| ISO9660/AFS/SAF | reutilizar | android/src/bt4_iso_source.c, bt4_iso9660.c; pread e fd próprios, sem extração |
| Catálogo ENG/ESP | reutilizar | port/src/bt4_base_profile.c; os três programas continuam obrigatórios |
| BPE/retrato | adaptar agora | sys/bpe.c, fight_panels_ui.cpp, bt4_eye_anchors.h; parsing limitado e saída RGBA nativa |
| Matemática/geometria | converter seletivamente | sys/math3d e col_a/b/c; fórmulas podem ser preservadas, tipos e acesso a memória precisam de adaptação |
| Estado de lutadores/combate/IA | converter por sistema | btl_input, btl_ai_*, btl_char_status_anim; remover dependência de globals, offsets fixos e layout PS2 com testes de comportamento |
| Modelos/animação/cenários | reconstruir ingestão | estruturas parciais e streams VIF no arquivo; exigir dados reais e semântica de cada atributo |
| Renderer | reconstruir | não ligar gs_core/gs_vu1/gs_gpu; conhecimento de materiais/shaders serve de referência, não dependência |
| Áudio | adaptar depois | decoders e formatos ADX/ADPCM são candidatos; não usar RPC/IOP como sistema de áudio |
| Controles | SDL/Android próprios | aproveitar conhecimento de ações/contextos; bridge libpad2 não é estrutura de entrada nova |
| Saves/menus/HUD | adaptar formatos/comportamento | formato de save e regras preserváveis; UI/render novo, sem memória snapshot PS2 |
| Heap/kernel/CRI como serviços PS2 | substituir | sem Port_Malloc, Port_Window, Game_Main ou SDK de sucesso fictício no novo link |

## Formatos e evidência disponível

O retrato foi escolhido porque possui caminho, checksum e conversão existentes.
O primeiro marco usa apenas esse formato específico, não um importador universal.
As estruturas disk são lidas com little-endian explícito; a expansão BPE usa
tamanhos de bloco big-endian. Nenhum offset de arquivo vira ponteiro absoluto.

Modelos têm evidência em `include/sys/gfxm_e.h`, gfxm_e_b/c/d e btl_obj:
meshOfs em +0x6C; registros de mesh variáveis com next, node e stream VIF em
+0x60; texturas via TexFile/TexEntry e offsets em palavras; poses/nós e
animações referenciados pelo recurso. Isso não basta para inventar um layout
linear position/normal/UV ou os pesos de skinning. VIF será tratado como
codificação do arquivo durante conversão, não como um VU1 executando no jogo.

Os candidatos concretos para personagem 0, traje 0, intacto são:

| Recurso | Global id | Origem calculada pelo código atual |
| --- | --- | --- |
| Retrato | 0x71C | PZS4US1.AFS membro 1819 |
| Modelo | 0x1612 | PZS4US2.AFS membro 2250 |
| Animação A | 0x141E | PZS4US2.AFS membro 1750 |
| Animação B | 0x1518 | PZS4US2.AFS membro 2000 |

Fontes: Bt4_ModelFile em src/battle/view_a_d.c (traje/variante alteram o id),
BtlObj_RequestCharaModel em btl_obj.c e tradução de ids em sys/file.c. Os números
não são um parser nem comprovam que arquivos não fornecidos foram inspecionados.
Para converter o modelo precisamos desses bytes privados, metadados do recurso,
interpretar atributos/índices, relação dos nós, bind pose, canais de animação e
materiais/texturas associados, e comparar o resultado com uma referência real.
A ISO não é distribuída. O novo app já lê a cópia do proprietário; uma próxima
etapa pode inspecionar esses membros com diagnósticos específicos.

## Arquitetura destino

- Content: fd SAF retido, streams bounded, blobs imutáveis e locators AFS.
- Assets: parser disk separado dos objetos; Image/Mesh/Skeleton/Animation/Material
  próprios, vetores e handles com geração, sem copiar structs de runtime PS2.
- Runtime: lifecycle Android/SDL, ownership RAII, tarefas/threads normais,
  cancelamento explícito e caches com orçamento; nenhuma troca manual de SP.
- Simulation: tick fixo, mundo/entidades próprios, estado de combate e eventos;
  migrar regras com referência e comparar numericamente, inclusive diferenças
  de floating point em relação ao soft-float anterior.
- Renderer: materiais/meshes nativos, transforms e skinning, filas de upload,
  pipelines Vulkan e tratamento de resize/device loss. O visualizador atual
  só implementa textura RGBA + quad de apresentação, não esses sistemas 3D.
- Platform: SDL áudio/input, armazenamento/save Android; estado salvo como dados
  versionados, não snapshot de ponteiros ou RAM do PS2.

## Sequência para preservar gameplay

1. Confirmar o marco de recurso original no Galaxy; preservar hashes/metadados
   e screenshot real de GPU. Não chamar isso de gameplay.
2. Ingestão do modelo, textura/material, esqueleto e uma animação reais; comparar
   geometria, pose, UV, alpha e timing com referência. Não usar cubo ou boneco
   substituto para marcar sucesso.
3. Movimento/voo e câmera com tipos nativos; casos de velocidade, aceleração,
   orientação e colisões com resultados de referência.
4. Combate/combo, hitboxes, dano, stun, defesa e IA por estados; depois
   transformações/fusões com eventos, recursos e custos verificados.
5. HUD/menus/efeitos, áudio, saves, controles contextuais e lifecycle completo.
   Cada diferença de semântica ou recurso ausente deve permanecer documentada.

Hoje nenhuma dessas etapas de gameplay/3D está implementada no alvo novo.
O código válido antigo é uma referência para migração, não é descartado nem
rotulado como port completo. O build não certifica execução no aparelho.

## Continuidade: preparação e índice BT4

A versão 0.6 implementa o índice de setores BT4 em DBZ4.BIN, ausente no reader
clássico reaproveitado na 0.5, cache transacional e investigação de metadados
do primeiro modelo. Consulte [PREPARATION.md](PREPARATION.md) para a correção,
contratos de integridade/retomada e bloqueios 3D. O jogo continua incompleto.

## Continuidade 0.7

O proprietário confirmou o retrato original em paisagem na 0.6, com distorção
em retrato. A 0.7 fixa a política Android/SDL, audita dimensões/viewport e
acrescenta o primeiro decoder/sampler nativo de movimento, sem personagem 3D.
Consulte [PRESENTATION_AND_MOTION.md](PRESENTATION_AND_MOTION.md).
