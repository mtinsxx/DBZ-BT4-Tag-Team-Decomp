# Núcleo integrado 0.8: contratos, evidência e limites

Base preservada: motor `a06b23d` e aplicativo `baf53b1` (0.7), na branch
`feature/android-native-engine-rebuild`. O destino é o jogo completo. Este
incremento implementa infraestrutura e algoritmos verificáveis que serão usados
por esse jogo; não conclui os marcos A–F nem introduz regras de combate próprias.

## Grafo executável atual

```text
SAF fd → ISO/DBZ4 + catálogo ENG/ESP → bytes limitados + CRC
         ↓                                  ↓
     retrato existente           modelo[3] / movimento A[1]
         ↓                                  ↓
 native-assets-v1             Skeleton / AnimationClip nativos
         ↓                                  ↓
         │                 codecs TNSK/TNAC → native-records-v1
         └───────────────┬──────────────────┘
                     AssetLibrary (handles com geração + leases)
                         ↓
                     Engine / Scene / entidades
                         ↓
                relógio racional 30 Hz / entrada
                         ↓
         canais → pose local/global → eventos → volumes de ataque
                         ↓
                 SceneFrame / lotes de eventos por tick
                         ↓
          SDL3 GPU Vulkan: apenas imagem, world identidade
          malha/material/skinning: BLOQUEADOS, sem fallback
```

O caminho Android de apresentação agora usa a mesma classe `Engine`, biblioteca
de recursos e snapshots de cena, em vez de passar uma imagem avulsa ao renderer.
A preparação de personagem carrega os registros na mesma arquitetura e exerce
pose/eventos em CPU. Não é uma submissão de personagem à GPU. O adaptador gráfico
atual rejeita uma cena com poses 3D, múltiplas imagens ou transformação que não
saiba desenhar; não ignora essas entidades para alegar sucesso.

## Interfaces e ownership

| Fronteira | Contrato implementado | Limite atual |
| --- | --- | --- |
| Platform → Content | JNI valida os três programas, retém fd SAF e serializa sessões | ISO não é copiada; threads de preparação não executam gameplay |
| Content → Assets | leitura limitada, CRC/tamanho do catálogo, offsets LE explícitos; conversores puros recebem `Bytes` | dados desconhecidos falham com offset; locators não são layouts especiais de Goku |
| Assets → Scene | `ImageId`, `ClipId`, `SkeletonId`, `EntityId` tipados, geração, lease imutável | single writer; ponteiros retornados pela cena são empréstimos até a próxima mutação |
| Scene → Animation | parent-first, canal pelo ID original do nó, transformações locais/globais | IDs repetidos ou >=71 são rejeitados; não se presume que cada mesh seja um osso único |
| Animation → Simulation/Battle | eventos por entidade/tick, máscara original, volumes mundiais | sem aplicação de dano, defesa ou decisão de ação |
| Simulation → Renderer | `SceneFrame` com leases de imagens e cópias de poses, tick explícito | backend de meshes/materiais/câmera ainda inexistente |
| Engine → Frontend | `gameReadiness()` retorna ausências explícitas | INICIAR JOGO continua desabilitado; nenhum retorno falso de jogo pronto |

`HandleTable` invalida IDs antigos ao remover; geração que dá wrap aposenta o slot.
Cenas detectam ciclo, parent inválido, profundidade >128 e overflow de matriz.
Atualizar transforms publica resultados somente após validar toda a hierarquia.
A biblioteca faz cópia profunda dos tracks preservando aliases internos, evitando
que um produtor altere um clip depois de publicá-lo. Leases impedem remover um
recurso em uso; referências de entidades a um recurso removido provocam falha
explícita. O caller deve destruir/reassociar entidades antes de descarregar assets.

Orçamentos iniciais: 64 MiB de payload CPU de assets, 4096 recursos por tipo,
4096 entidades, 8 ticks por apresentação. Não são um orçamento completo de RSS,
GPU ou draw calls. Eles precisarão ser medidos com meshes reais na Xclipse 940.
Não foi inventado um orçamento GPU ou uma redução de qualidade sem esses dados.

## Relógio e comportamento já convertido

O loop original termina com `Gfx_EndFrame(2)` e a sequência usa 30 ticks/s.
O acumulador nativo usa nanosegundos inteiros ×30: não acumula erro de um
`delta=0.033` aproximado nem depende de 60/120/144 Hz de apresentação. Trabalho
por apresentação é limitado, mas dívida não é descartada. Lifecycle suspenso não
acumula tempo em segundo plano. Falha de um tick trava o runtime em fault e
impede publicar snapshots parciais como quadro válido.

O relógio de batalha preserva 34/32/34 ms e saturação em 9:59:59.999.
O adaptador de entrada preserva conversão byte/stick, botão/comando held/pressed/
released, smoothing 0.5, normalização, stickTurn com wrap e contadores limitados
a 100. A entrada 255 existe no d-pad original e é aceita. A fase de loading
salta smoothing e congela contadores. A derivação de comandos contextuais,
ring buffer/replay/rede, bindings touchscreen e geração de comandos pela IA
**não foram portados**. SDL da apresentação ainda trata somente lifecycle/saída.

Modos de animação Once/Loop reproduzem `BtlObjAnim_Step`: Once fixa frame e
previous no fim; Loop volta ambos a zero e descarta overshoot. Tracks são ligados
pelo ID do nó; track ausente usa rest translation; rotation-only mantém essa
translação. Layer secundário ajusta seu tempo à duração do principal. Transição
usa smoothstep e exclui nó 0, conforme o original. A política genérica de duração
do componente não substitui flags e decisões de transição de um lutador original.
Look-at dos nós 2/46/47, mandíbula, root motion de combate e retargeting ainda
exigem sistemas próprios e comparação com o jogo.

As matrizes usam row vectors e `local * parent`: `Ref_Mtx_Mul(dst,a,b)` efetua
`b*a`, apesar dos nomes de argumentos antigos. A inversa rígida matemática da
rest pose é calculada; **isso não demonstra o espaço de bind dos vértices**.
`vertex_bind_space_verified=false`; o schema não aceita promover essa flag.
Não há pesos, índices ou bind pose de skinning inventados.

Eventos usam `int(frame+1/60)`, preservam o ajuste de previous igual a current,
cap de oito ANTES de mesclar argumentos por atributo e seleção das janelas de
hit. Todos os ticks de catch-up entregam lotes próprios, sem perder eventos
porque não houve draw entre eles. Volumes usam o mapa de 19 códigos de
`BtlObj_GetNodeByCode`, diferente de `BtlObjAnim_MaskToNode`, e fórmulas de
`BtlObjHit_BuildVolumes`/`col_a`/`col_b`: esfera ou OBB com offset e bounds
mundiais. Não incluem hurtboxes, broad phase, colisão de cenário, reação ou dano.

C++ usa `-ffp-contract=off` para preservar associação das expressões convertidas;
não se promete determinismo bit a bit entre bibliotecas matemáticas de plataformas
diferentes. São necessários testes diferenciais com referência comercial privada.

## Auditoria da execução original

Inventário reproduzível: [original-dependencies.json](original-dependencies.json),
gerado por `native/tools/audit_original_dependencies.py`. Registra hashes,
externs, definições, corpos literalmente vazios, marcadores INCLUDE_ASM e regiões
#if 0 em battle, IA, personagens, stage, efeitos, menus e binding de assets.
CI verifica que o inventário corresponde ao código. É inventário lexical, não
call graph completo nem medidor de completude semântica. Um corpo vazio pode ser
callback intencional; um marcador pode estar no ramo #else de implementação C.

### Ordem e estados

`Battle_Main` chama Init → Loop → Term. No Loop, scripts só avançam fora de pause;
pad, som, sequência e diversas tarefas têm regras próprias. Depois vêm
`BtlGame_PreUpdate`, `Battle_UpdateWork`, `BtlAiMgr_Update`, sample de input,
`Battle_Update`, `BtlGame_Update` e draw. Não é correto colocar todo o loop atrás
de um único `if (!paused)` e assumir equivalência.

`Battle_Update`, em ordem, chama input, main dos lutadores, scene, efeitos,
pós-scene, pós-scene dos lutadores, troca de stage, streaming de personagem,
streaming de objeto, stage, câmera 0, câmera 1, override de câmera, end-frame de
lutadores, objetos e visibilidade. `originalBattleOrder()` registra essa evidência;
não chama implementações vazias dessas fases.

`BtlChars_UpdateMain` passa sobre TODOS os lutadores para motion; depois stage6,
hold, snapshot1, stage7, snapshot2, stage8, snapshot3, stage9, snapshot4, câmera,
replay/FX, dano enfileirado, colisão, stage10 e clash. Esse ordenamento é requisito
para portar combate, em vez de atualizar um lutador inteiro antes do adversário.
Loading e pause afetam fases distintas; `BtlChars_EndFrame`, por exemplo, executa
BtlChange antes de retornar por pause. Hit-stop não é simplesmente suspender SDL.

`BtlSeq_Reset` escolhe a tabela por modo e chama enter do primeiro estado válido.
PreUpdate chama o callback pre; Update consulta o callback update, chama exit do
estado anterior e enter do novo. EXIT=99 pode reiniciar ou terminar conforme flags;
estado 3 é Fight. A nova engine ainda não implementa essa máquina nem a dependência
em HUD, scripts, round/reset, resultado e save. Não foi criado um combate substituto.

### Dependências que ainda impedem a migração completa

- `btl_char_*` depende de roster, snapshots, flags/action/command, colisão de stage,
  objeto/pose, câmera/target, efeitos, áudio, saves e regras de tag. Há C reutilizável,
  mas seus layouts/offsets globais não são contratos nativos.
- `btl_ai_*` depende de distâncias, alvo, ações disponíveis, regras de batalha e
  pedidos de entrada. Não pode gerar decisões corretas sem esses contratos.
- `btl_cam` depende de fighters, áreas/colisão de stage, cortes e script; não basta
  trocar sua matriz por uma câmera orbital de demonstração.
- `gfxm_e*`/`view_a_d` estabelecem seleção/relocação de modelo, partes/nós e texturas.
  Streams e renderer desktop informam programas/formatos, não um array universal
  de vértices nem a associação de atributos e pesos desse membro real.
- Há implementações condicionais: `BtlScene_IsEffectHidden` possui corpo C em PORT
  e INCLUDE_ASM no outro ramo. `ObjShadow_BuildPacket` inclui tentativa desativada
  por #if 0 e marcador assembly. Marcar ambos simplesmente como C completo seria
  incorreto. Menus/efeitos também contêm marcadores/rotinas parciais a investigar.

## Primeiro caminho de personagem e diagnóstico Galaxy

Os locators documentados fornecem IDENTIDADES de investigação, não um layout
particular codificado para Goku. O conversor usa model entry3, texture entry12 e
motion A entry1 com limites comuns. As identidades/tamanho/CRC conhecidos vêm dos
catálogos ENG/ESP. Motion B é validado e reportado, mas não convertido.

O scanner VIF trata os comandos como **dados de arquivo**: valida payload/STCYCL,
registra UNPACK (offset, destino, dimensões, bits, máscara, TOPS), referências TEX0
(dimensões/PSM/TBP/CBP) e chamadas de programa como marcadores. Salta bytes MPG;
nunca executa VU1. Sentinel só termina em fronteira de comando. Comando desconhecido
falha explicitamente. STCYCL zero/DIRECT zero permanecem não suportados.

No Galaxy, a ação de fundação de personagem produz:

1. Compatibilidade real ENG/ESP e SHA/bytes dos três membros.
2. Header/tabela/streams com offsets e histogramas limitados, mesmo se o esqueleto
   falhar (por exemplo IDs repetidos). Não exporta coordenadas ou blobs.
3. Skeleton e primeiro clip, se suportados, ligados à cena comum.
4. Reprodução CPU Once até min(last_frame+1,300) ticks, hashes de poses inicial/
   intermediária/final, contagem de eventos/volumes e erro exato se ocorrer.
5. Commit separado dos dois registros nativos e recarga na cena a partir do cache,
   repetindo o probe. Hashes permitem comparar conversão fresca e cache no aparelho.

Isso verifica processamento, não fidelidade visual, malha Vulkan ou gameplay.
O cloud não possui os três membros nem a ISO; não se declara que esses novos
parsers passaram sobre dados comerciais. O próximo bloqueio concreto exige esse
relatório para associar streams, nós e materiais, seguido de referência visual/
numérica para demonstrar posição/normal/UV/pesos e bind space. Não há conversor
que retorne sucesso para geometria desconhecida.

## Critérios de aceitação e estado

| Marco | Evidência atual | Falta para aceitar |
| --- | --- | --- |
| A | SAF/programas e retrato validados pelo proprietário na 0.6; nova cena/cache/clock testados no host | conjunto obrigatório real, cena funcional do jogo e nova execução no Galaxy |
| B | apresentação de textura existente + metadados VIF | malhas/materiais/texturas de modelo/cenários convertidos e desenhados |
| C | parsing/sampling, hierarquia e poses CPU com fixtures | vínculo real validado, bind/weights/skinning e animação visual original |
| D | adaptador de entrada e geometria de volumes isoláveis, conectados ao tick | movimento/voo/câmera, input SDL e colisão do mundo originais |
| E | eventos e volumes disponíveis aos futuros consumidores | FSM/combo/dano/defesa/ki/hit-stop/IA e regras originais |
| F | leitor e apresentação reaproveitáveis | menus/seleção/HUD/áudio/efeitos/save/transformações/fusões completos |

O loop experimental não chama `Game_Main`: o objetivo definitivo desta branch é
substituir seus contratos por sistemas nativos, sem relincar o grafo guest antigo.
Não foi resolvido retroativamente o mmap da branch compacta preservada.

## Validação deste incremento

Oito grupos host com ASan/UBSan: pixels contra decoder desktop, formatos,
cache antigo, apresentação, animação, VIF metadata, núcleo integrado e cache de
registros. Fixtures são sintéticos e exclusivos de testes. Novos casos incluem
hierarquia/ciclos/stale IDs, inversa/matrizes, binding de canais, layers, eventos
por tick, overflow/fault, 30 Hz em 60/144 Hz, volumes, schemas truncados byte a
byte, CRC/DBZ4 → dois commits → scene, morte do processo após o primeiro commit,
reuso sem leitura ISO, corrupção/perfil e limpeza segura. LeakSanitizer está
inativo no container ptrace; ASan/UBSan permanecem ativos.

ARM64 usa `--no-undefined`, ELF 16 KiB e checagem dos símbolos JNI/ausência de
Game_Main/Port_Window/GS guest, mais compilação Java. Shaders existentes SPIR-V
continuam compilados/validados. SDL e BPE/CLUT não foram alterados.

Push valida biblioteca/Java sem gerar APK automaticamente. O workflow_dispatch
`build_apk=true` gera um APK apenas quando há um objetivo integrado no Galaxy.
Logs/artefatos e limites devem acompanhar o resultado; build não comprova jogo.
