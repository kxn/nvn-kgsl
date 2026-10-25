> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# BUFFER资源48格式与实际FDL元数据40格式

修复真实D2R v14的RGBA32F/width65536/offset18002624在SetStorage被两格式profile拒绝的问题。构造/大小/对齐/存储类/descriptor CPU快照依照独立格式元素大小与既有SDK布局事实，和有限native GPU消费资格分开；不只添加游戏单张特例。48种有元素大小依据的普通颜色格式，39种有translator等价format query，实际FDL BUFFER元数据40种。packed8只开放CPU元数据；没有已核native映射则FDL仍拒绝。原47种image/volume范围保持。

BUFFER为连续width×cpp，flags0按声明common-kindfe对齐cpp（12B取4）；显式LINEAR/LINEAR_RENDER_TARGET分别32/128。对齐元数据不赋予BUFFER作为RT的权限。存储保留原pool/BO/address/offset，无shadow、CPU图片转换或重绑。CPU资源可以合法，而native FDL因64B prefix不能整除cpp等硬件表示限制明确拒绝；不能倒过来用native限制拒绝CPU构造。

根源码正常public checker双san各23,955项，涵盖实际v14形状、三种flags、48格式、view/注册、query/span、OOM/cache rollback、descriptor持有、NO_ACCESS及旧GPU拒绝。原BUFFER两recipe/FDL descriptor回归与API29全后端严格链接通过。新normal FDL库由7个原Mesa单元/69文件最小闭包重建，40格式/760 descriptors原FDL直接对照及原47image/volume回归通过；NDK独立严格重编，仍仅5个元数据exports、m/dl/c依赖，无GPU执行或renderer依赖。新normal记录附本目录，未倒写旧47记录。

合入时发现候选nvn.c含旧版本回退，会撤销已提交的builder生命周期修复；root去掉全部无关回退，仅保留BUFFER rawStorageClass一行flags变化。随后在同一根源码再次通过完整生命周期/旧getter/ABA/external/Window回归。root-merge-audit.json记录实际源指纹，候选只用于来源，不能把candidate文件哈希当根源码哈希。

本批没有实际GPU/Android执行，也尚未证明真实游戏新BUFFER构造成功。native BUFFER采样/Draw/RT/copy/Window等消费门禁保持；普通CPU登记成功不算GPU能力。下一版D2R须从原NSO重新生成、显式使用新FDL记录验证推进。入口数量仍406。
