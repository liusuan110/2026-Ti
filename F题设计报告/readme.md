# F题历史报告｜早期 CCD 方案草稿

本目录保存 2026 F《李萨如图形显示控制装置》的早期报告：采用线性 CCD 一维采样思路，队伍信息、系统框图、测试和标定等位置仍有占位内容。

**后续 UVC 摄像头方案、完整工程和成品报告入口见 [ti-last](https://github.com/liusuan110/ti-last)。** 两份目录名称相同，文件内容与阶段不同，本目录作为备赛资产保留。

## 文件与边界

- [main.typ](main.typ)：早期 CCD 方案正文，DDS 按 500 MHz / 20 倍 PLL 假设描述；不能套用到 ti-last 当前 25 MHz 配置。
- [电赛F题_设计报告.pdf](电赛F题_设计报告.pdf)：已有导出件，本次未重新生成或核对它与正文的同步程度。
- `figures/` 与 `package/`：本阶段插图和模板组件；[refs.bib](refs.bib) 在本次整理基线中为空。
- [LICENSE](LICENSE)：保留原模板许可证声明。

本稿不作为最终实物实现、比赛成绩或整机达标的证明。报告原件和插图保持原样，方便回顾方案变化。

## 模板来源

[原模板 README](https://github.com/liusuan110/2026-Ti/blob/baa2572bd9b8b3b52cd470acbf58d3a98b66d2b9/F%E9%A2%98%E8%AE%BE%E8%AE%A1%E6%8A%A5%E5%91%8A/readme.md) 属于上游 Typst 模板介绍，其中 April 的 2025 年参赛经历和获奖叙述不是本项目经历。

原说明基于 Typst 0.13.1，并致谢 [SandOcean-ovo 电赛 LaTeX 模板](https://github.com/SandOcean-ovo/Template-for-Electrical-Competition-Report) 与 [a-kkiri CUMCM Typst 模板](https://github.com/a-kkiri/CUMCM-typst-template)。本次未试编，学习模板优先看本库 [报告模板](../报告模板/README.md)。

返回[备赛资产首页](../README.md)或[资产索引](../docs/ASSET_INDEX.md)。
