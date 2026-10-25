> 历史文档 / Historical record. 本文保留当时结论与失败，不代表当前支持范围。当前状态以 [STATUS](../../../STATUS.md) 为准；旧证据的公开边界见 [HISTORICAL_EVIDENCE](../../../HISTORICAL_EVIDENCE.md)。本地路径和设备标识已替换，少量SDK原指令摘录已改为语义描述。

# Window interval matrix：官方 FrameTimeline 过滤核查

仅复用已完成 capture；没有 ADB、重采样、自定义计时器或 pacer 修改。原系统 trace **只留 ignored，不归档**。可归档本目录的 SQL、filtered CSV、report/child-evidence JSON、脚本及官方工具 stderr。

## 可确认的结果

真实 child evidence 为：N3 每个 interval 两代 `12+12=24` Present；N2 每个 interval `8+8=16`。总数 `6×(24+16)=240`，不是每 child20。N3 interval0 不调用 FrameTimeline tag，故24次没有 native tagged slice；其余预期216条。

`native-frames.csv` 有216条，按 `(upid,surface_frame_token,layer_name)` 各自唯一匹配 expected surface，并按 `display_frame_token` 各自匹配 actual/expected SF display，四种slice duration均完整。`layers.csv` 为22层：前5个进程各两层×12条，后6个进程各两层×8条。`child-summary.csv` 与串行矩阵顺序逐组相符。

**归属边界**：FrameTimeline-only trace 的 process.name/start/end缺失，未携带 qualifier nonce。CSV 的 slots/interval/nonce标签依据 capture.json 所记录 N3→N2、每批0..5的串行顺序，以及已 strict-stop 的 child evidence 顺序关联；不是独立捕获的 guest/host PC、nonce→PID身份证明。实际是11个 tagged PID、22个 layers，N3 interval0没有 tagged PID行。

官方分类：209条 `Late Present`、7条 `On-time Present`；全部 `on_time_finish=1` 且 `Valid Prediction`。`on_time_finish` 与 `present_type` 不是同一结论。surface actual slice 结束代表 app/GPU完成并post给SF，不能当上屏；CSV另列关联 SF actual/expected slice及差值，名称不冒充 GPU耗时或光学延迟。

**Trace health 未修复**：`trace_sorter_negative_timestamp_dropped=220`（error）；`frame_timeline_event_parser_errors=77`（info）。过滤组数量吻合不消除全局丢弃/解析错误，也不证明完整 cadence conformance。N3 interval0 cadence依然没有tag观测。探针逐帧生成/拷贝/只读核验、最终hold和HOME，非连续steady-state工作负载；不能由相邻slice间距推断理想NVN interval、刷新率或FPS。设备线程交错未强制；确定交错来自另有HOST合同测试。

## 重现

官方工具：`${D2R_REFERENCE_ROOT}/.local/tools/perfetto/trace_processor`，v58.2 commit `add693d8b338ba9599dbcbc3e300b1ab8c000897`。launcher、实际binary、trace、capture和每个child evidence哈希见 `report.json` / `child-evidence.json`。

```sh
${D2R_REFERENCE_ROOT}/.local/tools/perfetto/trace_processor query \
  -f .local/nvn-native/parallel/window-interval-frametimeline-v1/native-frames.sql \
  .local/nvn-native/window-interval-matrix-v1/frametimeline.pftrace
```

`analyze.py` 仅编排上述官方工具；`health/schema/native-frames/child-summary/layers/integrity` 各有独立 SQL、CSV、stderr及命令/哈希。`native-frames.sql` 只输出 `TX - nvn-native-direct%` 原生SC层及关联SF事件，没有归档其余系统帧。

Layer来源为当前 `nvn-native/android/present.c` 的 `ASurfaceControl_createFromWindow(...,"nvn-native-direct")`。字段解释参考与本机工具同commit的[Perfetto官方文档](https://github.com/google/perfetto/blob/add693d8b338ba9599dbcbc3e300b1ab8c000897/docs/data-sources/frametimeline.md)：app actual是完成/post范围，SF actual含display stack范围。该文档仍注明SurfaceView支持限制，不能据文档或本次ASurfaceControl样本推广到任意Android surface。此处只报告该设备实际可查询的原生子layer数据，不做普适支持声明。
