> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公开 canonical buffer/texture copy 接入

本批将已有 CopyBufferToTexture / CopyTextureToBuffer 入口接到正常编译的高通 CS 原始块内核。显式 normal kernel check → CMake header → 原 host bridge 注册 → 独立 NVN core；未提供检查记录时默认禁用。原有 Metal/Vulkan renderer 没有加入编译或后备路径。

这是 flags0、同 view format、2D/array canonical GOB 的有限子集，覆盖现有37格式的原始块。pixel region、mip、layer 和实际使用的 row/image 字节 stride 在 CPU 上核验，texel 数据只交给 GPU。缩放、格式转换、其它 flags、未知别名与未资格资源仍拒绝；这些是当前后端边界，不能当作 NVN 本身的限制。TextureToTexture 的 canonical 组合路径在本批仍未接入。

原始 pool/地址/存储不换绑。录制持有 texture/backing；CopyCommands 借用不可变 job 并独立保留资源；每次提交仍检查 public Window 所有权。源/目标真实范围与 top-byte-ignore 后的匿名 private backing 参与别名检查，不能仅凭不同 BO/地址宣称不重叠。所有仍被 published handle 或 active chunks 持有的命令区域均受保护，提交第一轮重复验证后来绑定的目标区域。

失败时回滚命令字节与公开资源 pins；private BO free 失败进入 retired owner，未知 GPU completion 继续由现有 KGSL busy 账本保活。本批也修复 common compute encoder 的失败 free owner 传播与 NULL metadata/map admission，错误不再丢掉未释放的 job BO。

正常 HOST 两 sanitizer 各 66,877 项检查：37格式、两个方向、mip/array/partial/odd-stride、CopyCommands、重复 mock Submit/Finish、callback source pin、旧句柄命令区域与后来绑定区域、分配/map/cache/pin失败回滚。Mock queue 仅持有资源，未执行 GPU 指令，也没有以 CPU 生成图形结果。独立 common encoder fault fixture 各5519项检查，26个失败 owner 均保留并重试释放。真实 API29 backend 严格链接及正常 CMake native driver/host bridge OBJECT 编译通过。

本批无 Android/GPU 执行证明，不能因此宣称 BC 上传已在 D2R 通过。最新真实 D2R 仍是 v7：原 ARM64模块到 CopyBufferToTexture errno95，没有游戏 Draw/Present/frame。GPU wholebatch qualifier 与真实游戏验证后续单独记录。
