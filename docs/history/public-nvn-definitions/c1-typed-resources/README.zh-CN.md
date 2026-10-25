> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# C1 samplerless typed resource合同

本报告仅核对原SDK静态代码、已有Maxwell shader decoder与公开硬件/FDL源码；没有执行游戏、GPU、ADB、CPU模拟器或Metal/Vulkan renderer。原SDK字节和窄范围反汇编只在本ignored目录。旧C1 `QuerySamplerType112=Texture2D`是原创离线specialization输入，不能用来确定D2R实际资源。

## 已确定的资源数据流

已冻结的 `../sdk-texture-cb-selector/ready.json` 证明本原SDK53.314的标准graphics QueueInitialize显式设置default texture CB selector2，绑定各graphics stage的CB2并kickoff。已有 `../c1-handle-source-research/metadata.json` 使用现有Maxwell decoder证明：C1 TidB110为TXQ dimension和2D TLDS，112为`Texture1DLodZero` TLDS。TidB是默认CB的DWORD offset，不是pool ID。

- FS/API stage1，TidB110 → CB2 byte0x1b8 → SeparateTexture selector10；112 → byte0x1c0 → selector11。原SDK SeparateTexture基址0x168、stride8。
- slot中的低DWORD载体解出texture descriptor ID（低20bit）。selector10/11、TidB110/112与pool descriptor ID是三个不同域，不能互换；有效ID须核实际pool count和reserved区（当前NVN policy从256起），不能把未知当0。
- TXQ/TLDS为samplerless，不要求SeparateSampler0；编译器旧JSON仍用combined sampled-image反射壳，并不证明实际shader读取TSC。完整guest64bit句柄与scalar/plural高word继承按此前carrier审计单独保存；TLD所需低32bit不能反推全部CB2已实现。

## 1D image / buffer / pool descriptor必须分开

公开NVN enum明确TARGET_1D=0、2D=1、BUFFER=10。Buffer texture仍由TextureBuilder/TextureInitialize和TexturePoolRegisterTexture构建，是带format、texel count和typed fetch的texture；它不是NVNbuffer对象本身，也不是任意GPU地址或SSBO。texture pool是含描述符的表，并非该纹理的像素存储。绑定完整路径必须从已绑定texture pool的具体slot找到注册快照和原始backing pool/range。

原SDK实证（原NSO SHA/build在sdk-evidence.json）：

- layout函数475b50中，475c64–475c94对target10单独分支，storage bytes=width×format表元素bytes；高度、GOB/普通image pitch不参与这个buffer分支。宽是texel数，必须独立checked乘法；release函数的32bit乘法不构成合法溢出许可。
- GetStorageAlignment475540先处理LINEAR_RT128/LINEAR32；buffer路径475558–4755a4取format元素bytes（12B改4B），且可能被device policy覆盖。不能从一个支路概括所有buffer对齐或直接复用GOB512。
- texture描述符生成491cf0在491df0区分target10；专用buffer width编码492130–492150写宽减1的上下16bit。原TexturePoolRegisterTexture4769d0调用这个描述符生成器，不把selector号当typed texture。

原厂[NVIDIA clb197tex.h](https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clb197tex.h)定义ONE_D与ONE_D_BUFFER(type6)，以及独立TEXHEAD_1D 32bit width-minus-one字段。MIT，属于硬件格式定义，不是NVN完整行为规格。

A650 public FDL `fd6_view.cc:580–635`的`fdl6_buffer_view_init`按format元素bytes得到elements，发TEX_BUFFER专用descriptor：长度分拆到WIDTH/HEIGHT字段、base向下64B、STARTOFFSETTEXELS表达子范围。它不使用普通2D row pitch。执行方必须保证向下对齐后descriptor所需完整范围仍归原BO owner，且offset按元素对齐；用真实FDL helper生成/校验，不能改2D template的height来伪造buffer。普通1D image是另一资源，即使宽和storage碰巧相同也不能合并。

## 原shader translator的真实分岔与当前编译缺口

只读shader模块 `Instructions/InstEmitTexture.cs:471–510`：原TLDS1DLodZero先查实际QuerySamplerType；若TextureBuffer就去LOD，保留buffer类型，否则将1D按现有`Sample1DAs2D`降为2D并补y=0/LOD0。`EmitTxq:1118`也使用实际type。`Spirv/Instructions.cs:1433,1512–1532`分别发整数坐标ImageFetch，以及image有LOD/Buffer无LOD的size query。

因此112原指令本身不能证明BUFFER或1D image。110既有2D TLDS和TXQ，首个受限resolver应要求非array、非MS 2D实际view；112只接受实际1D或BUFFER；其它组合应拒绝或单独经ISA/编译验证，不能拿archive的expected值倒推资源。

当前StrictAccessor `Program.cs:58–67,86–108`仅收2D并拒isBuffer，必须增加真实typed specialization与reflection校验后才能编译新包。1D的QuerySamplerType值须为guest1D，最终TLDS reflection可能因上述lowering为native2D，不能要求二者盲等。BUFFER则要保留Dim.Buffer。`Queries.cs`目前CoordNormalized=false也拒绝；TexturePass在intCoords判断返回前仍调用该query，因此必须明确输入真实descriptor坐标属性，不能省掉query或用fixed true冒充捕获。整数TLD本身不采样filter/wrap。

另一个严格边界：普通sampled declaration使用FP32 component type（Declarations.cs:179）。反射TextureFormat.Unknown是translator语义，不等于实际NVNformat可忽略。目前首子集应只接有真实format/转换证据的浮点结果格式（如已资格RGBA8 UNORM/R32F），SINT/UINT/深度/压缩等不得凭Unknown自动接收。不能把普通texture变成storage image，也不能用GetImageHandle替代。

## 可立即实现的resolver合同（尚不接生产）

1. draw录制快照须含graphics stage、SDK默认selector版本、guest完整carrier及低32payload、SeparateTexture10/11是否实际写过、绑定texture pool身份与descriptor slot版本。没有绑定/未初始化/旧handle/pool注销/跨device立即拒绝。
2. 从descriptor_entry的注册时owned snapshot取得resolved target、format、swizzle、view levels/layers、宽高深、samples、规范source layout、原pool/BO/offset/bytes。不要反读当前32B软件texture_slot猜类型：其现字段缺target/mips/view，且不是原NVIDIA TIC。view指定format/levels/layers必须先用同一resolve helper完成合法兼容性检查。
3. 以该实际typed snapshot回答QuerySamplerType/CoordNormalized；Buffer→TextureBuffer，1D→Texture1D，2D→Texture2D。query callback不能见archive expected值。对未支持query、format或实际native view拒绝；用完整source/control/C1+实际query形成specialization key，exact-match stage/pair，无firstvariant fallback。
4. 对2D/1D image由实际FDL image-view artifact提供typed descriptor、view尺寸/mip范围；原canonical GOB必须先经现GPU conversion与owner依赖到shadow。Buffer由`fdl6_buffer_view_init`专用native descriptor，宽为texels，不得套2D pitch/GOB转换。当前bridge只2D RGBA8 single-mip，故1D/buffer需要新增正常FDL view/资源owner子集并资格，不能仅让RegisterTexture返回成功。
5. Record/Submit/Finish保持descriptor快照、原pool/BO、shadow/conversion、private descriptor和shader owner。源可变时每次提交重新准备shadow；未知完成整组保活。不以“出现在resources中”代替实际读取依赖或写访问记录。descriptor slot重注册、source更新、handle重放都要明确版本与alias。

## 尚须真实D2R捕获的最小字段

- FS SeparateTexture10和11的实际scalar/plural绑定调用、64bit参数、低32payload与descriptor ID；当时texture pool identity/count/base及slot注册版本。
- 对应RegisterTexture的实际NVN target/format/view（format override、base/count mip/layer、swizzle、depth mode）、TextureBuilder flags/width/height/depth/levels/samples、canonical pool flags/offset/size/地址身份。
- C1完整code/control/C1身份与对应draw时上述状态，尤其112是1D image还是BUFFER，以及110的实际2D格式/尺寸/levels。不得拿第一个创建的纹理、旧2D实验或shader寄存器值代替。
- query size实际应取view后的texel dimensions/有效LOD，不是byte extent；Buffer length必须除format元素bytes且严格整除。首次可限single-mip，不能把未知levels默认1。

这些字段现有typed observer/调用捕获可补；本次未运行ADB/游戏，不声称已捕获它们。SDK版本合同只覆盖已核53.314 graphics，不泛指所有NVN版本/compute。

## 独立验证边界

离线正例必须同一原code/control/C1分别给显式112=1D与BUFFER：正常frontend→spirv-val→SPIR-V反射应出现native2D height1路径与Dim.Buffer差别，QuerySamplerType及完整specialization key不同；原2D包字节/默认行为不变。未知type/缺state/重复key/array/MS/integer unsupported应失败，不生成成功artifact。

后续GPU原创测试分别用1D image和typedbuffer、非零offset、不同宽/format、有效integer fetch坐标、query size/LOD、swizzle、只读/guard、同handle重放与slot重注册验证，独立oracle从原创texel表按整数索引和已证format转换取值。1D image测试经真实GOB/shadow路径，Buffer用FDL专用buffer descriptor，二者不能互代。不得用CPU读源值当GPU expected，不得用归一化/双线性采样作为TLD oracle；越界fetch行为未有完整NVN定义时不作任意固定0 oracle。

原C1还包含其它UBO/support资源与坐标数学，typed resource编译通过不等于完整C1 shader或真实D2R NVN Draw可执行；该完整资格须另闭合resource/UBO实际绑定。

本报告的冻结复现包留在工作目录 `.local/nvn-native/parallel/c1-typed-resource-contract/`；原SDK字节、反汇编和完整私有proof不进入docs。
