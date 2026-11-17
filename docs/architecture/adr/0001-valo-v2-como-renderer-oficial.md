# ADR 0001 — VALO V2 como renderer oficial

- **Estado:** aceita como direção; migração ainda não concluída
- **Data:** 2026-11-17
- **Contexto:** a shell principal usa V2, mas V1 continua registrado no módulo QML, é compilado e aparece em fallback e testes. V2 também depende do tipo de matriz std140 declarado no cabeçalho de dados V1.

## Decisão

Convergir para uma única implementação óptica V2. Migrar cada consumidor e testar sua paridade antes de retirar a implementação V1. Uma interface pública ou contrato ABI que precise continuar existindo deve ser preservado por um adaptador de compatibilidade sobre V2 ou por um contrato de dados independente, sem manter um segundo renderer ativo. A retirada do adaptador depende de uma política explícita de compatibilidade e de consumidores conhecidos.

O protocolo Wayland versionado `z_systalia_surface_v1` não é o renderer V1 e não deve ser removido ou renomeado como parte dessa limpeza. Preservar apresentação direta e ciclo de vida dos buffers, os layouts GPU necessários (incluindo 128, 144 e 160 bytes enquanto houver dependência), os tokens, serviços, nomes públicos e recursos QML até que uma migração equivalente seja demonstrada. `SystaliaTheme` é utilizado por componentes V2; mapeamentos e cores precisam ser inventariados e migrados ou mantidos como compatibilidade, sem alteração visual arbitrária.

## Consequências

A exclusão exige referências rastreadas, baseline e testes de build, CTest, QSB e execução Wayland real. O fallback V1 deve permanecer até que o caminho V2 equivalente tenha validação de conteúdo, eventos, renderização e ciclo de vida. O sufixo `v1` em protocolo, dados ou testes não é critério de exclusão. O problema conhecido de conteúdo preto XWayland continua uma limitação identificada, não evidência de paridade.
