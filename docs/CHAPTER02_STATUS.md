# 第二章编写与验证记录

2026-09-12：完成图文初稿。尚未发布或建立章节标签。

2026-09-19：完成最小 Echo/Add 联合仿真 Demo。Host 通过 SystemC Bridge 提交 `7 + 5`，请求经过深度为 1 的 FIFO，设备延迟 3 ns 后产生结果，Host 取回 `12` 并与 gem5 Guest 的正常退出一起验收。

2026-09-13：新增自定义指令小节，明确 RV64 custom-0 下阻塞 Echo/Add 的教学编码，补充 `.insn`、C 内联汇编、寄存器约束和 gem5 字段译码说明。异步 API 仍是语义设计，不能将 status 和完整的 64 位 value 默认映射到一个 rd。

正文为 `chapter-02-cpu-systemc-command-bridge.md`，Word 为同名 DOCX，配图在 `images/chapter-02`，同时保留 Draw.io 和 PNG。

本文参考现有项目的 CPU 指令桥接、请求队列、完成表和等待唤醒机制，以独立的 Echo/Add 接口重新组织叙述。没有公开核心计算 Engine、产品指令编码、地址布局或内部源代码。上游参考链接用于说明框架机制，不代表上游自带本文的 NPU 扩展。

## 文档检查

四张 PNG 已逐张检查，并保留可编辑源图。更新后的 Word 已通过 Microsoft Word 导出 PDF，逐页检查正文、编码表、代码块和图注，共 12 页。标准渲染脚本能完成转换，但其 LibreOffice 环境存在中文缺字问题，故以 Microsoft Word 实际渲染为排版依据。

## 实现状态

最小 Demo 位于 `examples/chapter-02/echo-add-systemc-host`，已经完成 `custom-0` 的 `npu_add` 译码、Guest 寄存器加法和退出码验证，也完成 gem5 + 外部 SystemC Host 的请求接受、设备延迟、结果保存和取回。当前两条路径仍是并行验证：`npu_add` 在 gem5 内立即完成标量加法，SystemC Bridge 请求由 Host 侧示例线程提交；自定义指令尚未直接驱动 SystemC Bridge。

正文包含 Guest 汇编与 C 封装示例，以及模型接口伪代码。已按字段计算核对 `0x02B5060B`，但当前检查的 PATH 工具链无 RISC-V 编译目标，尚未完成汇编或 C 示例的交叉编译验证。总在途额度、带代际信息的 handle 等是本文建议的教学接口设计，不能作为现有工程已经实现的保证。

## 发布前联调

- 验证阻塞 Echo/Add 教学编码与 Guest 封装；进一步落实异步 ABI 的状态与数据返回方式、错误及消费语义。
- 在明确的 CPU 模型上验证一次自定义指令提交、PC 重入、非推测执行、等待唤醒及统计行为。
- 验证 Echo/Add、请求队列满、完成表压力、响应消费、非法 handle 与拥有者检查。
- 覆盖完成先于 wait、wait 先于完成及同时间边界，验证无丢失唤醒。
- 明确时钟采样与请求/返回延迟，记录 accept / issue / done / visible / consume 时间。
- 验证退出和 drain，包括未消费结果、在途请求与挂起 hart。
- 在干净依赖环境执行，补充实际命令和输出后，再把文章升级为可复现教程。
