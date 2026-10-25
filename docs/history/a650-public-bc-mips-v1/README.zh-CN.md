> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 公共NVN四mip BC/sRGB：实际typed Program、late binding与存储生命周期

Adreno650、普通APP_UID、Android14。8种BC1/2/3 UNORM/sRGB、64²与127×65两尺寸、每源4mip、三组view及同handle数据改变重放，48handles/96次实际Submit和Finish。384次原创mip上传，192次late registration，240次已登记CPU对象Finalize，96次提交后descriptor替换/旧backing EBUSY/Finish后回收，全部成功。检查不保证CPU更新和硬件采样每次确实重叠，不将其称为时序性能测量。

实际typed注册成功并强制typed_archive=true；通过正常完整source/control/C1匹配、实际公开descriptor/query和精确variant选择。使用新BUFFER格式族后的正常FDL库，及正常typed-v3 pipeline；未接旧renderer/CPU图像转换/软件BC解码。公开caller未调用私有encode或raw submit。1572864颜色通道、786432目标guard bytes、7667712源/metadata只读字节通过，最大RGB差1、alpha严格相等。完整96份目标BO共2359296字节，ZIP重新逐文件解压核验长度与SHA。正常runner verified，finally强停并确认退出。

资格范围为ordinary2D、nearest/no-mip/clamp、各selected base0..3的相对LOD0；不证明mip filtering、所有LOD、array、3D或完整游戏。a767原graphics control资格为0x880，不冒称完整0x898游戏域。本次没有真实游戏模块执行或首帧。每submit仍做既有全mip转换/owner构造，不声称已优化性能。

当前根源码正常HOST通过C/NumPy端点1572864通道对照、45个collector正负例、5个无效builder组合，以及实际API29完整CMake/KGSL/FDL/Activity严格链接；默认颜色和旧singlemip BC probe机器码保持。HOST和synthetic验证不算GPU执行。

同时修正一个旧BUFFER query fixture：RGBA8_SNORM现在有合法metadata QueryTextureFormat26，原必拒断言改为成功值；hostSnorm capability仍false、未知格式和native消费拒绝仍检查。根源码正常checker43项、双san及NDK通过。附独立消费缺口审计：当前RGBA32F只有资源/FDL元数据，真正BUFFER Draw需typed-buffer binding和匹配实际query的新正常shader variant，不能扩旧DWORD descriptor后直接宣称支持。
