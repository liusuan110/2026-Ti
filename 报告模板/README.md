# 设计报告模板(LaTeX + Typst,任选)

> 队里谁顺手用谁。两个都是**电赛专用开源模板**,拿来当起点改。整理:2026-06-06。

## 两个模板

### 🟢 `Typst-nuedc-typst-template/`(推荐:上手快)
- 来源:[github.com/zhao-leo/nuedc-typst-template](https://github.com/zhao-leo/nuedc-typst-template)(**MIT**,2025-08 更新)
- 编译:装 [Typst](https://github.com/typst/typst)(单个二进制,无需 TeX 发行版)→ `typst compile main.typ` **秒出 PDF**;需 **Typst ≥ 0.13.1**
- 文件:`main.typ`(主)、`cover.typ`/`abstract.typ`(封面/摘要)、`figures/`、`refs.bib`、`package/`、`tests/`
- 优势:编译快、不折腾格式、改起来直观

### 🔵 `LaTeX-nuedc_template/`(需 TeX 发行版)
- 来源:[github.com/chenboshuo/nuedc_template](https://github.com/chenboshuo/nuedc_template)(**CC BY-NC-SA 4.0**)
- 编译:**XeLaTeX + 中文(CTeX)**;主文件 `report/main.tex`,样式 `report/contestTemplate.cls`;`report/` 下带 `makefile` / `make.ps1`
- 说明:原仓库有个子模块 `docs/doxygen-awesome-css`(仅给 Doxygen 代码文档做 CSS 美化,**与报告无关**),浅克隆未含、不影响写报告;残留 `.gitmodules` 已删
- 作者自述"无参赛经验,仅供参考",当起点改即可

## ⚠️ 套用前对照官方设计报告格式

- **评分 6 项**:① 方案设计与论证 ② 理论分析与计算 ③ 电路图与设计文件 ④ 测试方法与数据 ⑤ 结果分析 ⑥ 报告工整
- **常见结构**:封面 → 摘要(≤300 字)+ 关键词 → 方案论证(比较与选择)→ 理论分析与计算 → 电路与程序设计 → 测试方案与结果 → 结论 → 参考文献/附录;正文 ≤ 8000 字
- 模板里缺的章节自己补;数字指标对照原题(原题见本仓 `历年三类赛题题库/`)

## 边界(老实说)

- 两个都是**轻量个人项目**;我只做了 clone + 去 `.git` + 加本说明,**未实测编译**。Typst 装好二进制基本能编;LaTeX 需本机装 TeX(如 TeX Live/MiKTeX)。要我在那台机器上**试编译出 PDF**可以说一声。
- 模板版权归各原作者(见各自 `LICENSE`/`README`),仅团队学习参考。
