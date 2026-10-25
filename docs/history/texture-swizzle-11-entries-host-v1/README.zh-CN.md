> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Texture sampling metadata: swizzle, depth/stencil mode, Compare

本批以已交付 texture-subresource-registration-candidate/source-root-v3 为精确基线，最小增量8文件，11个公开 ABI 入口。父分支正在真机 v11 期间，本包只写 ignored 快照，无 tracked/ADB/游戏逻辑修改。

## 独立依据

- 本机既有 texture-sdk-layout/facts.zh-CN.md 和 static.json：SDK53.314 TextureBuilder 默认 swizzle={R2,G3,B4,A5}、depth/stencil mode=DEPTH0；builder 是参数存储，allocation/consumer 后续资格独立。
- 已纳入仓库的 texture-view-sdk-research.zh-CN.md，以及原 ignored texture-view-sdk/facts.zh-CN.md/static.json：View presence 与字段值分离；getter 无条件写 out 再返回 presence；defaults 全部未指定；Compare 先比较 presence，再只比较已指定字段，忽略无效 payload 和 padding；注册时 view 未指定字段继承 texture。
- C ABI 和 enum 以仓库公开 MIT reconstructed nvn.h 为准；公开 C++ 重建只用于交叉核签名。公开声明来源：https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn.h.html 及 https://smo.monsterdruide.one/docs/classnvn_1_1TextureView.html 。声明不替代行为证据。

此次没有新反汇编原SDK、没有原游戏执行、没有从 Metal/Vulkan 实现/其他渲染后端复制模板。所有 ABI 都在既知508中，不猜未知15。

## 完成的接口

TextureBuilder Set/GetSwizzle、Set/GetDepthStencilMode（4）；Texture GetSwizzle/GetDepthStencilMode（2）；TextureView Set/GetSwizzle、Set/GetDepthStencilMode（4）；TextureViewCompare（1）。冻结基线实际394个公开入口，本候选405；已知未实现114→103，未知15不变。这个数字指入口/本组完整元数据合同，不表示405个调用全部已有所有硬件语义。

Builder 保存所有公开合法 swizzle枚举0..5与mode0..1；Texture 初始化按值快照，getter 不因 builder 后续修改/复用改变。SetDefaults 恢复 identity和DEPTH。TextureCompare 也比较有效swizzle/mode，显式identity与默认identity对资源等价。

private texture_desc 明确存 swizzle_set：无显式值时内部全零结构解释为identity；显式 ZERO,ZERO,ZERO,ZERO 为 swizzle_set1+四个0，绝不当默认。View 另有独立 swizzle/mode presence，仍在公开40B opaque内（private36B）且无pointer。默认 view GetSwizzle 写四个ZERO并返回false，不能把它当有效identity override；显式identity返回true。其 private 编码不声称与原SDK内部字节兼容。

TextureViewCompare 忽略未指定payload与全部padding；公开输入同一语义的view即使不同栈地址、memcpy移动、padding不同仍相等。NULL和无效私有编码的错误策略属于本后端防御约定，不冒称NVN非法输入保证。其他现有消费者仍保留本后端private zero-tail检查，它不参与Compare。

登记快照继承Texture值，逐项采用View presence override；完整canonical source owner和source_layout以及原资源extent/offset不变，已交付selection继续独立保存mip/layer范围。Builder、View后续改写、Texture Finalize均不能改变已登记快照。BUFFER/LINEAR的既有软件登记resolver也携带同一sampling metadata。

## GPU与复制边界

两条现有采样路径在实际descriptor/query、pin和GPU分配前拒绝非identity swizzle和mode!=DEPTH0。不默默将自定义映射忽略或错发identity。当前保守采样资格对所有格式都要求mode0；不是宣称NVN的color格式必须mode0，也不是已经实现depth/stencil采样。

本批没有实现GPU swizzle、格式reinterpret、typed storage image、alias权威写回或新的mip/array sampler。native shader消费者需要把所选值实际传至真实FDL view/descriptor并完成格式分量/深度平面资格后，才能放开此门。

raw byte copy、offset查询和cache范围不把swizzle当CPU颜色转换：它们仍使用原始存储地址、format和原有region规则；未创建新CPU图像转换/CPU图形输出。此包新增完整能力是公开元数据合同及consumer安全门，非新的GPU采样资格。

## 正常验收

source-root-v2/nvn-native/tools/check_texture_swizzle_metadata.py 走实际core与RAM backend，GCC ASan/UBSan和Clang函数签名trap；实际NDK29完整KGSL、BC0/BC1严格链接，BC1使用原normal FDL verify所得真实静态库与源码闭包。未改normal FDL记录provenance。

穷举全部6^4=1296 swizzle，逐个两种resource mode，从真实PFN store/get、资源初始化/immutable getter、View override/复制、descriptor快照到consumer fail-closed；验证显式全ZERO、identity、每一presence类别的值差异、unused payload/padding相等、无效枚举原子回滚与原资源字节未变。同步回归已交付子资源登记与现有source_resource。

最终结果见check-v2/check.json；v1先完成同一生产代码的正常全链路通过，v2仅补强每一Compare字段差异用例，不修改生产语义。所有历史freeze保留。HOST没有执行GPU或原游戏，NDK链接不是Android实际执行；真机测试由父任务统一合并后进行。
