# F题早期报告：用一条扫描线观察图形

本目录保存 2026 F题《李萨如图形显示控制装置》的早期报告，采用线性 CCD 扫描示波器屏幕的思路。它关注一个核心问题：断开信号源与装置的电连接后，怎样从屏幕迹线判断状态，再调整 DDS 输出。

阅读时可以重点看题目分析、系统方案和自动模式流程，观察图形要求怎样转化为交点数量、幅度跨度和对称性等反馈量。图表、标定、测试和参考文献还留有待补内容，反映了这一阶段的工作进度。

## 从这里读

- [已有 PDF](电赛F题_设计报告.pdf)：快速浏览这一阶段的报告。
- [main.typ](main.typ)：阅读或继续修改正文。
- [figures](figures/)与[package](package/)：插图和排版组件。

后续改用 UVC 摄像头观察整幅屏幕，完整工程与方案见 [ti-last](https://github.com/liusuan110/ti-last)。早期稿采用 500 MHz DDS 系统时钟假设，后续工程采用 25 MHz PLL 旁路配置，对照时可沿着这个差异查看驱动与硬件设置的变化。

## 模板来源

排版组件及 [LICENSE](LICENSE) 沿用原模板，[原说明](https://github.com/liusuan110/2026-Ti/blob/baa2572bd9b8b3b52cd470acbf58d3a98b66d2b9/F%E9%A2%98%E8%AE%BE%E8%AE%A1%E6%8A%A5%E5%91%8A/readme.md)保留作者介绍和参赛随笔。继续研究排版可进入本库[报告模板](../报告模板/README.md)，其中集中存放 Typst 与 LaTeX 资料。

返回[备赛导读](../README.md)或[资产索引](../docs/ASSET_INDEX.md)。
