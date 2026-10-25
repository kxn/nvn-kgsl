# 可公开的历史 GPU 证据

这里实际保存了7组原创 native GPU夹具的完整raw读回压缩包，包括原depth396读回失败版本；共62,300,160字节解压数据，3,904,908字节ZIP。另有50份64×64 AHB/window 原创输入产生的测试像素，共819,200字节。没有游戏场景/贴图、原Maxwell shader、control/C1、衍生SPIR-V/NIR/IR3或SDK数据。window历史测试用了翻译过的原shader，这里只保存简单渐变/角点夹具像素，不保存shader；不能将它标成原创shader或真实游戏画面。

每个readbacks-manifest.json记录每成员bytes/SHA256，并与旧已归档manifest逐成员核验一致。result-summary.json只提取实际设备结果和独立验证的安全scalar；正常build/source记录ZIP没有直接迁入，因为它们有时通过嵌套record或完整源码snapshot携带不应公开的原game/SDK数据。全部原始记录和payload在私有交接目录保留。

- SSBO192：真实192Submit/Finish、576份BO读回，4,416,768通道误差0、86,016个storage字精确一致。原创raw-native，不是公共SSBO或游戏GPU。
- 深度源采样60：真实60Draw/180Submit/60Finish，60raw；原创shader，不是公共NVN像素。
- depth396/depth432/canonical-depth648：各自整组实际GPU结果独立保留，不能将canonical/zeroCOLOR资格混为一项。depth396的首次坐标错误raw和失败结果没有删除。
- Gather96：原创shader整组GPU，不代表原Maxwell/Gather公共路径。
- sampler576：原固定误差oracle失败（最大3、原阈值2），结果仍保留false。后来现成Khronos CTS高精度模型对同一576raw做离线验证通过，CTS参考结果单独保存；没有重跑GPU、没有把CTS当NVN规范或抹掉原失败。

historical-local-manifest.json是完整1564文件历史资料的相对路径/大小/SHA清单。zip-member-audit.json逐项记录所有77个旧ZIP的8795成员，实际读取、解压和hash过，包括检查嵌套ZIP（本批没有嵌套ZIP）。它们是hash目录，不表示所有payload都公开或都能公开。不要从私有目录自动同步整个archive到公开仓库。

这些都是历史资格；新repo源文件和路径变化后，应重新正常构建、产生新的build-record再部署。旧结果不重新标成新源码执行证明。所有GPU证据均不能代替真实原D2R Draw、Present或首帧。
