# Paisagem, integridade de pixels e primeiro componente de animação

## Evidência do aparelho e causa encontrada

O proprietário confirmou em 2026-11-07 que o retrato original do Goku na 0.6
aparece proporcional em paisagem no Galaxy S24 FE. Em retrato, observou forte
compressão horizontal. Isso confirma o marco de recurso real naquela execução,
não modelo 3D, gameplay ou correção da nova versão. Não foram fornecidos logs
0.6 de dimensões nem screenshot/bytes do recurso para comparação no cloud.

A Activity já declarava sensorLandscape no Manifest. Porém o próprio SDL 3.4.12,
em SDL_androidwindow.c:Android_CreateWindow, chama Android_JNI_SetOrientation.
SDLActivity.setOrientationBis escolhe FULL_USER para uma janela resizable sem
hint válido. Assim, só o Manifest não fixa a política do runtime. Não há evidência
de erro BPE/CLUT: não modificamos native/src/portrait.cpp nem a versão do cache.

A antiga fórmula fit estava matematicamente correta em ambas as orientações.
Sem os tamanhos físicos do diagnóstico não atribuímos o achatamento a ela por
suposição. A correção remove o override de orientação e torna explícitos/observáveis
os contratos de superfície, swapchain, viewport e transições.

## Implementação 0.7

- SDL_HINT_ORIENTATIONS="LandscapeLeft LandscapeRight", antes da inicialização.
- NativeEngineActivity solicita SENSOR_LANDSCAPE antes de criar a superfície e
  sobrescreve o ponto de extensão setOrientationBis no código do aplicativo.
  Não modificamos ou contribuímos patches ao SDL. O launcher de seleção de ISO
  mantém a orientação Android normal; a engine abre em paisagem e permite as
  duas posições de paisagem. Multi-window/políticas do OS ainda podem limitar
  orientação, portanto não presumimos que width sempre é maior que height.
- Cada quadro consulta dimensões lógicas e SDL_GetWindowSizeInPixels, além da
  extensão retornada pela aquisição da swapchain. Diferenças não são esticadas:
  libera-se a imagem por submissão de clear, espera-se idle e renova-se a claim.
  Um conflito persistente para com erro visível após um limite de recuperação.
  Não se cancela um command buffer depois de adquirir swapchain (proibido por SDL).
- Fit uniforme usa pixels reais. O viewport é o retângulo central do recurso;
  o shader não aplica um segundo fator de aspect. Scissor cobre o alvo inteiro.
  Retrato 256x512 em 2340x1080 -> 540x1080, x=900; em 1080x2340 ->
  1080x2160, y=90. Barras têm a cor de clear; nenhum eixo é esticado sozinho.
- Resize, mudança de display/orientação e retorno ao primeiro plano invalidam
  o alvo próprio e geram nova captura após a primeira submissão. SDL gerencia
  a swapchain; nossos alvos usam sua extensão efetiva. Pipeline fixo RGBA8 não
  depende da orientação nem de cache de driver. Device loss continua um erro
  explícito, não uma recuperação fictícia.
- native-presentation.log registra requested/config orientation, rotação,
  superfície Android e decor; native-engine.log registra logical/drawable/swapchain,
  target/source, geração, viewport e ratios. A exportação inclui ambos.

## Comparação dos pixels: escopos diferentes

O teste host extrai literalmente o decoder de retrato de
port/src/gs/fight_panels_ui.cpp, construindo uma referência apenas para testes.
Com uma fixture BPE não uniforme de todos os texels/canais de paleta, os 524288
bytes RGBA nativos coincidem com esse leitor desktop. A referência usa seu
scratch antigo; ela não é ligada nem distribuída no APK. Isso compara algoritmos
sobre os mesmos dados sintéticos, não uma captura original comercial.

No aparelho, antes da apresentação, um passe Vulkan no tamanho nativo usa
nearest, sem blending nem redimensionamento, e compara todos os bytes do
readback com a imagem RGBA do cache. native-pixel-check.txt informa SHA-256 dos
dois lados e quantidade de bytes diferentes. Falha interrompe o teste com erro.
Esse passe só testa upload/shader/orientação de UV; não prova a interpretação
dos dados comerciais contra o jogo original, nem contém um recurso substituto.
A captura posterior mostra o alvo de apresentação com barras e alpha composto.

Não podemos afirmar comparação direta com a imagem original do Goku no cloud:
a ISO/membro 1819 e uma referência visual original não foram fornecidos aqui.
A observação do proprietário é preservada como tal. A nova execução física da
0.7 e os hashes/tamanhos são necessários para confirmar a correção no Xclipse.

## Continuação real da engine: canais de animação nativos

animation.hpp/cpp implementam estruturas próprias de 64 bits, independentes das
structs PS2 e sem guest memory. Evidências: include/battle/bobj_a.h,
BtlObjAnim_Load/SampleRot/SamplePosRot/SamplePose/UpdateEvents em bobj_a.c,
Quat_Unpack/Quat_Slerp em sys/math3d.c, Mathf_Acos em mathf.c.

O decoder aceita um movimento BPE, lê o header de 0x94, 71 offsets u16 em palavras,
track flags/count e as duas disposições documentadas:

- rotation-only: quaternions u64 seguidos dos frames u16;
- translação/rotação: chaves de 24 bytes (xyz float32, frame u32, quaternion u64).

Os quaternions usam smallest-three: três componentes de 20 bits e índice do
componente removido em bits 60..63. Reconstituem-se floats nativos. Não há uso
de VU0 para isso. A amostragem interpola posição linearmente e rotação por SLERP
com caminho curto e limiar do código original. Track ausente usa rest pose;
rotation-only conserva rest translation; uma chave é constante. Fora do intervalo
de um track com várias chaves, preserva-se o comportamento original do sampler:
identidade e translação zero no track completo, não clamp automático.

Eventos mantêm atributos u64, frame, argumento e offset original. Volumes de
hit-begin são convertidos a vetores/radii próprios; bytes desconhecidos são
preservados, sem atribuir semântica de gameplay. Isso não implementa combate,
emissão de eventos por tick, animação em camadas, retargeting, fusões ou IA.

Há bounds, rejeição de NaN/Inf em campos usados, quaternions inválidos,
truncamentos, frames não crescentes e tracks sobrepostos distintos. Tracks que
compartilham o mesmo offset compartilham dados nativos imutáveis. Limites:
8 MiB expandidos, 262144 chaves únicas, 71 canais. O antigo validador desktop
silencia alguns tracks opcionais inválidos de BT4; o novo decoder falha
explicitamente nesses casos. Não se declara compatibilidade universal, nem se
aplica rest pose para esconder uma interpretação não comprovada.

A investigação opcional da ISO agora converte o primeiro movimento de
pzs4us2/01750.bin em memória e reporta duração, eventos, canais, chaves e teste de
amostragem do nó 0. Uma falha informa node/key/offset quando aplicável. O clip
não é marcado como recurso obrigatório preparado, nem salvo usando o manifesto
do retrato. Ainda não há personagem 3D ou animação visual implementados.

## Bloqueio seguinte e validação

Para malhas reais faltam semântica comprovada de índices/atributos nos streams,
pesos, bind pose, relação com os nós e materiais. Não tratamos a lista de meshes
como esqueleto automaticamente: o código permite vários parts para o mesmo nó.
São necessários os relatórios/recursos reais e comparação com referência antes
de ligar skinning, retargeting e desenho. A pipeline de cache do retrato não
substitui a reconstrução desses sistemas e INICIAR JOGO permanece bloqueado.

Cinco grupos locais ASan/UBSan passaram: parsing original, cache, aspect fit,
comparação desktop de pixels e conversão/amostragem de animação. Os testes
incluem respostas analíticas de interpolação, todos os índices de quaternion,
aliases, eventos, truncamento de cada byte, campos desconhecidos e erros.
O build ARM64 com --no-undefined e o empacotamento passam. Nenhum teste automatizado
é apresentado como teste físico do Android/Vulkan ou animação real do BT4.
