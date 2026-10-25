> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 原始 CASC shader 清单与真实图形阻塞定位

复用仓库既有CASC reader，从原RomFS正常核验162个shader资源：完整archive哈希、encoded范围/哈希、decoded长度/ckey MD5、工具及显式state来源。原v5契约识别10,825个容器，完整code/control/C1+stage去重后6,901个独立身份；22个magic候选不满足当前契约单列。native stage1/5分别3699/7124项，另stage2/3各一项尚未支持。清单数量不是已经编译或完成GPU资格的数量。

v16原游戏首个图形组合的两个完整2176B control，与原资源vs_fullscreen_v.bin（另两同内容别名）和ps_generate_depth_mips_p.bin完整匹配。原资源给出512B VS、512B FS及256B FS C1；当前未捕获GPU池代码本体，下轮ProgramSetShaders仍必须通过完整source/control/C1比较，不凭control hash直接接受。实际失败前的原模块执行与34纹理成功见相邻v16归档。

显式测试资源状态2D normalized、SeparateTexture10/SeparateSampler3下，两stage正常前端及SPIRV验证通过；当前正常IR3桥仍明确拒绝Gather，因此本目录不是native shader/link/GPU成功。实际shader含Gather与FragDepth，C1不删除、不折叠。Gather候选另外隔离验证，深度附件运行时仍未完成。没有把声明或shader编码中的selector当实际pool ID/资源格式，也没有伪造captured Draw state。

新入口只为显式source ID pair链接，不做6901×6901笛卡尔积；默认只建元数据清单。既有NSO提取共享同一v5规则，原6容器完整code/control/C1及state逐项相同。HOST19检查/16负例包括边界、短control、unknown stage、原CASC读取、别名去重、未知ID及合法ID位于未选资源时拒绝。checker成功只证明输入/失败证据一致，不代表编译失败被算通过。

source-inventory-metadata.zip仅含完整inventory.json元数据，已重新解出核SHA；不含原游戏binary、shader、control、C1或ASM。其余归档保存正常checker/编译来源及失败原因。共享producer指纹已改变，旧stage/pipeline记录对新部署应严格判stale，必须正常重新生成，不能改写历史记录。
