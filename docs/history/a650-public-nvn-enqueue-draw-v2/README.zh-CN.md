> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# 提交时快照改造后的公开 NVN Draw 回归

当前生产源码、正常合并 fullbase+C1 unit shader archive 和唯一 native views 正常构建后，在普通 APP_UID / A650 真机重新运行公开 NVN GOB Draw：3尺寸（64²、127×73、2880×1620）×3VFD×4绘制模式×2重放，共72次真实提交/Finish通过。原始 color VS/FS 及 API UBO0→原CB3映射；同handle第二次重放改UBO。449,180,832个颜色通道与独立离线oracle完全相同，13,437,792B canonical padding和589,824B outer guards保留。正常当前源码的enqueue快照路径实际执行，不沿用旧APK结果。

完整72份真实canonical pool读回lossless归档，逐文件长度/SHA256见raw-manifest.json；原始shader code/control/ASM未归档。实际完整读回462618624B。测试进程force-stop及pidof确认退出。首个root运行命令因系统Python缺numpy在ADB前拒绝，原日志保留于ignored目录；随后使用现有项目venv正常执行，没有修改runner或构建记录。

这是原shader的资格绘制，没有执行游戏模块，不是D2R帧；本次不证明C1 sample recipe、descriptor动态更新场景或所有16native pairs已获GPU资格。
