> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

本审计只读当前正式源码及正常制品元数据，未读原SDK CPU/ASM、未读旧Metal/Vulkan渲染器，未改生产源码，未ADB。原BUFFER11文件冻结包不改；root已指出它的nvn.c差异误带反向lifecycle改动并在集成时恢复。后续补丁必须对冻结、显式持有的baseline逐文件生成，clean apply不构成语义最小证明。附带新候选仅一个测试文件。

当前能力须分成五层：

| 层 | 当前事实 | 不代表的能力 |
|---|---|---|
| Public CPU资源 | 48种普通颜色格式，width×实际cpp，原pool/BO/GPUVA/offset、typed登记/pins | 没有BUFFER Draw/RT/texels/copy资格 |
| runtime FDL metadata | 40种真实fdl6_buffer_view_init描述符；47 image/volume独立 | 没有采样执行、BO范围或依赖证明 |
| 独立recipe/binder | 仅RGBA8(37)/R32F(10)，cpp4，borrowed原BO | helper没有进入公共pipeline，且不是GPU资格 |
| 实际source queries | BUFFER type2；39种QueryTextureFormat映射，RGBA32F46→34 | 编译器可接受的显式format仍只有25/10 |
| 公共Draw/native pipeline | BUFFER均明确拒绝；原2D、BC、3D路径独立 | 不能因前四层成功宣称BUFFER采样已支持 |

精确门禁和不足（行号/源SHA见facts.json）：

- src/texture_buffer_resource.inc:7/50/62/75：构造、存储、初始化、登记均走真实BUFFER快照，descriptor slot是32B软件身份tag，不是Maxwell TIC或Adreno16DWORD表。
- src/texture_buffer.c:7–26：recipe限format37/10、element_bytes4，独立验证FDL模板；bind:28–55验证borrowed owner、64B aligned-down prefix及范围。它调用compute_image.h:21的nn_cs_buffer_descriptor；该公共计算helper写死bytes/4和(address&63)/4。因此RGBA32F不能只删两格式if：会把texel数量和起始偏移错误按DWORD算。应让新typed BUFFER消费直接使用actual nn_fdl_buffer结果/专有typed binder；保留已资格的计算SSBO DWORD helper不变。
- src/nvn.c:306：gpu_object_access拒BUFFER；legacy gpu_late_draw_access:356也拒BUFFER。generic sample不是靠把这些全局if删掉闭合：src/nvn_pipeline_draw_prepare.inc:207要求actual selected source map.type3、:213要求actual target2D。必须独立新增type2/target10分支。
- include/nvn_native/pipeline.h:13/24：sample仍是nn_pipeline_image；只有附加2D FDL与volume FDL替代字段，没有typed buffer binding。pipeline.c:173把actual NIR dimension与volume?3:2比较，:185/212和:280默认按image验证/patch。因此即使绕过core gate，BUFFER也会被拒，或错误当image。native增加buffer字段时必须与legacy/BC/volume互斥，精确stage.type2并actualNIR Buf，而不是height1/pitch伪装。
- pipeline.c:299已把实际sample BO加入submission.refs；BUFFER新分支需沿相同有界refs/pins/unknown owner规则、完整FDL prefix owner证明与producer依赖，提交后才能退休。不应为连续BUFFER做canonical untile/shadow或改变原GPUVA。
- samplerless BUFFER应无sampler mask/无filter/wrap；C1 FS nativebinding2的actualNIR仅5个TXF。原frontend资源列表遗留sampler条目不等于实际NIR sampler消费；正常recipe的sampler_mask=0才是这里的硬件语义证据。

已存在的真实Maxwell变体（不把这些testspecialization称实际D2R绑定）：

1. 当前graphics-production-enqueue-typed-pipelines-v3有12stages/16pairs。stage11(request457ef920...)使用原C1 FS/code1792+C1 256和原VS，T110=Texture2D3、T112=TextureBuffer2。pairs5/13，binding1 actualNIR2D/TXF+TXS，binding2 actualNIR Buf/TXF。table维度[2]=1只说明坐标维数：1D和BUFFER都可为1，不能单凭它区分；必须同时验证exact offline stage.textures[].Type2/actual target10。
2. stage10(request59e3f80...)的T112 QuerySamplerType1并不代表nativeBUFFER：lowered NIR此版本已lower为2D，pairs4/12表也是真2D。不能用source Texture1D字样覆盖lowered事实。
3. stage11要求QueryHostSupportsSnormBufferTextureFormat=true；当前pipeline_frontend.c:103固定false。正常exact resolver会拒绝此variant，这是正确门禁，不能为复用现码改capability或firstfallback。
4. 另一个正常graphics-c1-buffer-format-stages-v1及buffer-format-integrated-v1/package支持显式false+QueryTextureFormat25(RGBA8)/10(R32F)。两个source variant SPV/native bytes均与旧BUFFER变体相同，因为非SNORM无需插入归一化；requirement身份仍不同且应保留。
5. 当前shader/maxwell/Queries.cs:55–68只接受显式R8G8B8A8Unorm/R32Float，RGBA32F尚不能正常生产false+QueryTextureFormat34的variant。译码器TexturePass.cs:28/664先问capabilityfalse再问actualformat，只有SNORM真正插归一化。RGBA32F可沿现真实分支正规增一项枚举并重新生成正常stage/package/types；不得手改SPV、recipe或“machinecode应该相同”来免除正常验证。
6. 原C1 sourceCB1作为immutable256B由Program owner上传；sourceCB3→public Uniform0→nativeUBO3。新资格保留这些真实映射，不能把C1当guest Uniform1或支持buffer0的假数据。T110/112的已证CB2 word偏移映射SeparateTexture10/11；仍要用实际绑定快照查询，不能从刚创建BUFFER资源的顺序猜它在11。

RGBA32F当前已能证明什么：

- Public format46、elementbytes16、QueryTextureFormat34分别是独立domain；runtime FDL使用真实PIPE_FORMAT_R32G32B32A32_FLOAT。
- v14实际创建width65536/offset18002624；逻辑bytes1048576。该offset模64=0、模16=0，但实际base还须来自对应BO；不能把这个常数当通用对齐或binding证明。
- FDL40格式测试含RGBA32F descriptor/尺寸/30bit elements、49bit地址、64Bprefix；Android仅ELF编译链接，不是设备执行证明。
- 当前没有“此BUFFER供哪个原Program/selector/descriptor ID使用”的actual D2R证据，也没有public BUFFER采样GPU输出。v15应保持观察真实public SetPool/Bind/registered snapshot，而不是预先认定C1/T112。

最小下一闭包（等root实际frontier，当前不实现）：

A. 正常compiler接受明确R32G32B32A32Float查询，不改原默认或旧record。以既有原C1 fixture作为authored specialization补false+34，保原code/control/C1身份，正常extract/translate/package生成真实Buf NIR/types、exact query requirements。保持T110实际2D。没有actual绑定则这仅是资格variant，不宣称game支持。
B. 公共prepare在exact resolver之后明确分支type2/targetBUFFER、mip1/layer1/noMS/identity view、float-result格式46（可同批回归37/10）；由actual registered pool/BO/offset/bytes创建 nn_fdl_buffer。独立typed native field承载descriptor+owner范围，禁止image字段混用。检查FDL base前缀全部属于仍live caller pool，保原storage与地址。fdl required_span不是新allocation要求，而是完整owner范围验证。
C. pure nativepreflight/encode从actualstageType2+Buf事实与typedbuffer匹配；samplerless recipe mask0，不插TSC，不解压/untile；复用现真正texture table upload/immutable job refs/queuecache协议；record-time绑定，enqueue-time登记晚绑定快照/pins，绝不late materialize读取可变slot。RT仍RGBA8路径。
D. 最小实际publicNVN资格用真实Device/pool/BUFFERTexture/Register/SetTexturePool/BindSeparateTexture/Program/Draw/QueueSubmit/Finish，原C1 pair显式qualified specialization，不能rawhelper代替publiccall。可以先小width与65536宽各一个，cpp16所有通道不同、跨64B prefix偏移16/32/48、非零pool offset；输入0/1/边界附近可精确表达float，独立原shader公式计算RGBA8输出，不以GPUsource读回或roundtrip作oracle。
E. 两次samehandle replay前通过合法Buffer alias写/Flush更新input，覆盖record后登记、同ID换registered resource、源TextureFinalize后descriptor仍持pool、queuebusy/unknown completion保活、缺typedtable/错误types/错误poolID/offset-prefix越界拒绝。采集完整RT+guard及source只读shadow，宽65536必须实际访问高位索引才算验证其count；取样坐标须由所选原shader/实际UBO合法生成，不能改机器码。
F. 若原C1公式不能形成有界独立oracle，可先另authored native samplerBuffer shader做硬件subset探针，但其通过不能代替上述originalMaxwell/publicNVN资格。没有相应actualguestbinding时native实现不firstfallback。

附带TEST-ONLY补丁：test-only-candidate/minimal.patch只改tests/d2r_bridge/pipeline_buffer_format.c。把旧RGBA8_SNORM必拒预期改为明确query26，保留未知0/UINT_MAX负例、capabilityfalse及其他检查；没有改compiler/生产门禁。旧负例会额外逐32输出验证错误清零，新成功仅验证count/value，所以fixture检查数353→321是预期，未删其它检查。实际运行现有check_pipeline_buffer_format.py：正常package重新生成/verify、两套san各321 HOST checks、43工具断言、四frontend负例以及NDK29 --no-undefined均PASS。无GPU、无公共登记/消费成功新增。
