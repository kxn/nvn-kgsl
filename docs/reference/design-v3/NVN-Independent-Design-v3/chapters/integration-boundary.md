# 接入边界：通用 NVN 库、宿主适配与可选游戏策略

本文件回答“是不是必须给每个游戏写一堆hack”。目标是**通用NVN兼容库＋薄宿主接入层**，不是另造D2R专用渲染器。本次仅设计，不执行接入或开发。

## 1. 对外一对一，对内不逐条机械发包

库提供NVN函数名、函数调用ABI、对象布局profile、返回值和命令语义。游戏仍通过BootstrapLoader/GetProcAddress等入口取得函数，宿主把调用重定向到此库。setter可能只更新host字段，Bind记录规范化状态，若干draw可合成原生状态块/执行片段；一对一的是外部合同，不是GPU包数量。

正式库不能只覆盖函数名：SDK对象大小/inline字段、command memory可观察布局、shader二进制、texture pool和GPU地址都是兼容边界。v3已经将未知签名/布局列门禁，不能用524行清单代替已完整实现。

## 2. 宿主适配应按运行环境，不按每个游戏重复实现

拟定HostServices责任：

- GuestAddressSpace：客体CPU地址访问、GPU虚拟地址与pool映射通知，不能将guest地址强转host指针。
- HostAccess/Observer：CPU读写观察或显式lease桥；游戏合法CPU/GPU同步的发布边界。
- GuestCallBridge：内存扩容callback的调用约定、当前guest线程上下文和重入约束。
- SurfaceBridge：独立实验窗口/离屏目标、acquire/release与生命周期。
- Diagnostics/Capabilities：错误、未支持语义、设备/ABI profile与资源预算。

NVN Program的NVIDIA shader到NIR/native编译链应属于通用库能力，不是每游戏重写shader的外置补丁。D2R离线shader包可用于首轮实验，但不能因此省去未来通用Program翻译的设计。

## 3. 两类宿主的接入成本不同

### 已在NVN API层截获

nxnative/D2R类原生运行环境或具有NVN HLE的模拟器，可以替换入口解析目标并提供HostServices。CPU架构/调用桥仍由宿主负责，库不承担CPU模拟。初次接入可能需按SDK版本识别入口，但不应为每个游戏改渲染器。

### 只在nvdrv/GPU提交层截获

传统低层GPU模拟器拿到的是Maxwell命令流而不是NVN调用。它不能仅加载本库就替换现有Vulkan backend。可选：
1. 增加NVN HLE截获，转到本库；对直接提交GPU命令的路径另处理。
2. 保留Maxwell method解码器，将其有序语义/资源映射接到本库执行层。

第二条是另一套前端，需要自己的decoder与语义映射，不属于本次首个NVN函数库验证版本。不能把两条路线的兼容范围混写。

## 4. 三种“特殊处理”要分开

| 类型 | 例子 | 是否基础能力 | 放在哪里 |
|---|---|---|---|
| 正常NVN语义实现 | shader ISA转换、GOB/native布局、别名、同步、query | 是；不是游戏hack | 通用core/backend/compiler |
| 二进制/SDK兼容profile | 对象inline字段、已知命令patch格式、handle编码 | 某些调用者需要；应尽量版本化/通用化 | ABI/CommandStorageAdapter，严格匹配 |
| 游戏/质量优化 | native资产包、BC→ASTC、FXAA跳过、bloom降质、UI重放、外推 | 否；不作为通用库的基础前提 | 可选D2R adapter/quality profile |

识别特定shader后做可证明等价的优化可以逐步泛化为IR模式pass；仅因游戏名就改变shader结果、删draw/barrier或伪query不是通用实现。raw command patch若最终有完整编码支持，也应从per-game模板迁到版本化decoder。

## 5. 实验中必须分别报告两种结果

- 通用模式：不依赖D2R专用资源/画质hack，验证普通NVN调用、shader、资源与同步。
- Qualified模式：显式启用native资源包或游戏策略，分别报告额外CPU/GPU/RAM收益和兼容限制。

仅第二种跑通，不能宣称通用NVN库已经成立。未来换一个符合已支持ABI和能力的游戏，应主要是宿主接线与兼容资格，而不是再写一套游戏渲染逻辑。
