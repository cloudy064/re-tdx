#!/usr/bin/env python3
"""Build a compressed, dependency-free TDX block explorer as one HTML file."""

from __future__ import annotations

import argparse
import base64
import gzip
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Sequence

import extract_tdx_blocks as extractor


HTML_TEMPLATE = r"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="color-scheme" content="light">
  <title>通达信板块图谱</title>
  <style>
    :root {
      --ink: #13231d;
      --muted: #66756f;
      --paper: #f4f1e9;
      --panel: #fffdf8;
      --line: #dedbd1;
      --green: #0e5d47;
      --green-2: #17755a;
      --mint: #dcece4;
      --amber: #c98223;
      --red: #b84a43;
      --blue: #336da6;
      --shadow: 0 18px 60px rgba(34, 46, 40, .10);
      --radius: 18px;
    }

    * { box-sizing: border-box; }

    html, body { min-height: 100%; }

    body {
      margin: 0;
      color: var(--ink);
      background:
        radial-gradient(circle at 7% -10%, rgba(201, 130, 35, .13), transparent 25rem),
        radial-gradient(circle at 96% 7%, rgba(14, 93, 71, .12), transparent 30rem),
        var(--paper);
      font-family: "Segoe UI", "Microsoft YaHei UI", "PingFang SC", sans-serif;
      font-size: 14px;
    }

    button, input, select { font: inherit; }

    button { color: inherit; }

    .masthead {
      position: relative;
      overflow: hidden;
      padding: 30px clamp(20px, 4vw, 64px) 28px;
      color: #f8f7f1;
      background: linear-gradient(118deg, #0d3429 0%, #0e5d47 58%, #14705a 100%);
    }

    .masthead::after {
      position: absolute;
      width: 480px;
      height: 480px;
      right: -150px;
      top: -310px;
      border: 1px solid rgba(255,255,255,.16);
      border-radius: 50%;
      box-shadow:
        0 0 0 52px rgba(255,255,255,.035),
        0 0 0 104px rgba(255,255,255,.025);
      content: "";
      pointer-events: none;
    }

    .brand-row {
      position: relative;
      z-index: 1;
      display: flex;
      align-items: flex-end;
      justify-content: space-between;
      gap: 24px;
      max-width: 1500px;
      margin: 0 auto;
    }

    .eyebrow {
      margin: 0 0 8px;
      color: #aed4c4;
      font-size: 12px;
      font-weight: 700;
      letter-spacing: .18em;
      text-transform: uppercase;
    }

    h1 {
      margin: 0;
      font-family: Georgia, "Songti SC", "SimSun", serif;
      font-size: clamp(30px, 4vw, 50px);
      font-weight: 600;
      letter-spacing: -.03em;
    }

    .subtitle {
      max-width: 650px;
      margin: 12px 0 0;
      color: #cde2d9;
      line-height: 1.7;
    }

    .snapshot {
      flex: 0 0 auto;
      padding: 10px 14px;
      border: 1px solid rgba(255,255,255,.18);
      border-radius: 999px;
      color: #dbece5;
      background: rgba(255,255,255,.07);
      backdrop-filter: blur(10px);
      white-space: nowrap;
    }

    .stats {
      position: relative;
      z-index: 1;
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      max-width: 1500px;
      margin: 24px auto 0;
    }

    .stat {
      min-width: 150px;
      padding: 12px 15px;
      border: 1px solid rgba(255,255,255,.14);
      border-radius: 13px;
      background: rgba(255,255,255,.07);
    }

    .stat strong {
      display: block;
      font-size: 20px;
      font-variant-numeric: tabular-nums;
    }

    .stat span { color: #b9d7cb; font-size: 12px; }

    .shell {
      display: grid;
      grid-template-columns: 330px minmax(0, 1fr);
      gap: 18px;
      max-width: 1500px;
      min-height: 680px;
      margin: 22px auto 46px;
      padding: 0 clamp(14px, 3vw, 42px);
    }

    .sidebar, .content {
      border: 1px solid rgba(31, 58, 47, .09);
      border-radius: var(--radius);
      background: rgba(255, 253, 248, .92);
      box-shadow: var(--shadow);
    }

    .sidebar {
      display: flex;
      min-height: 680px;
      max-height: calc(100vh - 48px);
      flex-direction: column;
      overflow: hidden;
      position: sticky;
      top: 18px;
    }

    .side-controls {
      padding: 16px;
      border-bottom: 1px solid var(--line);
    }

    .mode-switch {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 4px;
      padding: 4px;
      border-radius: 12px;
      background: #ece9df;
    }

    .mode-switch button {
      padding: 9px 10px;
      border: 0;
      border-radius: 9px;
      background: transparent;
      cursor: pointer;
    }

    .mode-switch button.active {
      color: white;
      background: var(--green);
      box-shadow: 0 5px 14px rgba(14, 93, 71, .22);
    }

    .search-wrap {
      position: relative;
      margin-top: 12px;
    }

    .search-wrap::before {
      position: absolute;
      top: 10px;
      left: 12px;
      color: #7d8984;
      content: "⌕";
      font-size: 20px;
      line-height: 1;
    }

    input[type="search"] {
      width: 100%;
      height: 42px;
      padding: 0 12px 0 38px;
      border: 1px solid var(--line);
      border-radius: 11px;
      outline: none;
      color: var(--ink);
      background: #fff;
    }

    input[type="search"]:focus {
      border-color: var(--green-2);
      box-shadow: 0 0 0 3px rgba(23,117,90,.12);
    }

    .families {
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
      margin-top: 12px;
    }

    .families button, .chip-button {
      padding: 5px 9px;
      border: 1px solid var(--line);
      border-radius: 999px;
      color: #56635e;
      background: #fff;
      cursor: pointer;
    }

    .families button.active {
      border-color: var(--green);
      color: var(--green);
      background: var(--mint);
    }

    .block-view-switch {
      display: flex;
      align-items: center;
      gap: 4px;
      margin-top: 11px;
      color: var(--muted);
      font-size: 12px;
    }

    .block-view-switch > span { margin-right: auto; }

    .block-view-switch button {
      padding: 5px 9px;
      border: 1px solid transparent;
      border-radius: 8px;
      color: var(--muted);
      background: transparent;
      cursor: pointer;
    }

    .block-view-switch button.active {
      border-color: var(--line);
      color: var(--green);
      background: #fff;
      box-shadow: 0 3px 10px rgba(34, 46, 40, .06);
    }

    .result-summary {
      padding: 10px 17px;
      color: var(--muted);
      font-size: 12px;
      border-bottom: 1px solid var(--line);
    }

    .result-list {
      flex: 1;
      overflow-y: auto;
      scrollbar-color: #bcc7c2 transparent;
    }

    .result-item {
      display: block;
      width: 100%;
      padding: 12px 16px;
      border: 0;
      border-bottom: 1px solid #ece9e1;
      text-align: left;
      background: transparent;
      cursor: pointer;
    }

    .result-item:hover { background: #f3f7f3; }

    .result-item.active {
      background: linear-gradient(90deg, var(--mint), #f8fbf9);
      box-shadow: inset 3px 0 var(--green);
    }

    .tree-family-heading {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      padding: 11px 14px 8px;
      border-bottom: 1px solid #e7e3da;
      color: var(--muted);
      background: #f6f4ee;
      font-size: 11px;
      font-weight: 700;
      letter-spacing: .05em;
    }

    .tree-node {
      display: grid;
      grid-template-columns: 27px minmax(0, 1fr);
      align-items: stretch;
      border-bottom: 1px solid #ece9e1;
    }

    .tree-node.root { background: #fbfaf6; }

    .tree-node .result-item {
      min-width: 0;
      padding: 9px 11px 9px 2px;
      border-bottom: 0;
    }

    .tree-node .family-tag { display: none; }

    .tree-node.root .result-title { font-size: 14px; }

    .tree-node:not(.root) .result-title {
      color: #32423b;
      font-weight: 600;
    }

    .tree-node .result-meta { margin-top: 3px; }

    .tree-toggle {
      display: grid;
      width: 27px;
      padding: 0;
      border: 0;
      place-items: center;
      color: #73817b;
      background: transparent;
      cursor: pointer;
    }

    .tree-toggle span {
      display: inline-block;
      font-size: 19px;
      line-height: 1;
      transition: transform .14s ease;
    }

    .tree-toggle.expanded span { transform: rotate(90deg); }

    .tree-toggle:disabled {
      color: #c8cec9;
      cursor: default;
    }

    .result-title {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      font-weight: 700;
    }

    .result-meta {
      display: flex;
      align-items: center;
      gap: 7px;
      margin-top: 5px;
      color: var(--muted);
      font-size: 12px;
    }

    .count {
      color: var(--green);
      font-size: 12px;
      font-weight: 700;
      font-variant-numeric: tabular-nums;
    }

    .quote-badge {
      display: inline-flex;
      min-width: 58px;
      justify-content: flex-end;
      font-size: 12px;
      font-weight: 800;
      font-variant-numeric: tabular-nums;
    }

    .result-title-side {
      display: inline-flex;
      align-items: center;
      gap: 8px;
    }

    .change-up { color: #b63832 !important; }
    .change-down { color: #17755a !important; }
    .change-flat { color: #66756f !important; }
    .numeric {
      text-align: right;
      font-variant-numeric: tabular-nums;
    }

    .content {
      min-width: 0;
      min-height: 680px;
      padding: clamp(20px, 3vw, 36px);
    }

    .loading {
      display: grid;
      min-height: 560px;
      place-items: center;
      text-align: center;
      color: var(--muted);
    }

    .loader-ring {
      width: 38px;
      height: 38px;
      margin: 0 auto 15px;
      border: 3px solid #d7e4dd;
      border-top-color: var(--green);
      border-radius: 50%;
      animation: spin .8s linear infinite;
    }

    @keyframes spin { to { transform: rotate(360deg); } }

    .detail-head {
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 22px;
      padding-bottom: 22px;
      border-bottom: 1px solid var(--line);
    }

    .detail-kicker {
      display: flex;
      align-items: center;
      gap: 8px;
      margin-bottom: 9px;
    }

    .detail-title {
      margin: 0;
      font-family: Georgia, "Songti SC", "SimSun", serif;
      font-size: clamp(28px, 4vw, 44px);
      font-weight: 600;
      line-height: 1.1;
    }

    .detail-code {
      margin: 8px 0 0;
      color: var(--muted);
      font-variant-numeric: tabular-nums;
    }

    .primary-button {
      flex: 0 0 auto;
      padding: 10px 14px;
      border: 0;
      border-radius: 10px;
      color: white;
      background: var(--green);
      box-shadow: 0 7px 20px rgba(14,93,71,.2);
      cursor: pointer;
    }

    .primary-button:hover { background: var(--green-2); }

    .family-tag, .relation-tag {
      display: inline-flex;
      align-items: center;
      padding: 4px 8px;
      border-radius: 999px;
      font-size: 11px;
      font-weight: 700;
      white-space: nowrap;
    }

    .family-0 { color: #7d4b07; background: #fae9cb; }
    .family-1 { color: #385d91; background: #dfeafb; }
    .family-2 { color: #8c3b36; background: #f6dfdc; }
    .family-3 { color: #6b4e93; background: #eae2f5; }
    .family-4 { color: #176048; background: #dcece4; }

    .relation-direct { color: #176048; background: #dcece4; }
    .relation-descendant { color: #6c5b37; background: #eee8d8; }

    .metric-grid {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 10px;
      margin: 18px 0;
    }

    .metric {
      min-width: 0;
      padding: 14px;
      border: 1px solid var(--line);
      border-radius: 13px;
      background: #faf8f2;
    }

    .metric span {
      display: block;
      color: var(--muted);
      font-size: 11px;
    }

    .metric strong {
      display: block;
      overflow: hidden;
      margin-top: 5px;
      font-size: 17px;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .relations {
      display: grid;
      gap: 10px;
      margin: 18px 0;
    }

    .relation-row {
      display: flex;
      align-items: flex-start;
      gap: 12px;
    }

    .relation-label {
      width: 58px;
      padding-top: 5px;
      color: var(--muted);
      font-size: 12px;
    }

    .chip-group {
      display: flex;
      flex: 1;
      flex-wrap: wrap;
      gap: 6px;
    }

    .chip-button:hover {
      border-color: var(--green);
      color: var(--green);
    }

    .table-tools {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      margin-top: 24px;
    }

    .table-tools h3 { margin: 0; font-size: 17px; }

    .tool-fields {
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .table-search {
      width: min(280px, 38vw);
      height: 38px;
      padding: 0 11px;
      border: 1px solid var(--line);
      border-radius: 9px;
      outline: none;
      background: white;
    }

    select {
      height: 38px;
      padding: 0 28px 0 10px;
      border: 1px solid var(--line);
      border-radius: 9px;
      outline: none;
      background: white;
    }

    .table-wrap {
      overflow-x: auto;
      margin-top: 10px;
      border: 1px solid var(--line);
      border-radius: 13px;
    }

    table {
      width: 100%;
      border-collapse: collapse;
      background: #fff;
    }

    th, td {
      padding: 11px 13px;
      border-bottom: 1px solid #ece9e1;
      text-align: left;
      white-space: nowrap;
    }

    th {
      position: sticky;
      top: 0;
      z-index: 1;
      color: var(--muted);
      background: #f7f5ef;
      font-size: 11px;
      letter-spacing: .04em;
    }

    tr:last-child td { border-bottom: 0; }
    tbody tr { cursor: pointer; }
    tbody tr:hover { background: #f3f7f3; }

    .code-cell {
      color: #33433c;
      font-family: "Cascadia Mono", Consolas, monospace;
      font-variant-numeric: tabular-nums;
    }

    .unresolved { color: var(--red); font-style: italic; }

    .pager {
      display: flex;
      align-items: center;
      justify-content: flex-end;
      gap: 8px;
      margin-top: 12px;
      color: var(--muted);
      font-size: 12px;
    }

    .pager button {
      padding: 6px 10px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #fff;
      cursor: pointer;
    }

    .pager button:disabled { opacity: .4; cursor: default; }

    .empty {
      padding: 44px 20px;
      color: var(--muted);
      text-align: center;
    }

    .footnote {
      margin: 22px 0 0;
      color: var(--muted);
      font-size: 12px;
      line-height: 1.7;
    }

    .hidden { display: none !important; }

    @media (max-width: 900px) {
      .brand-row { align-items: flex-start; flex-direction: column; }
      .snapshot { white-space: normal; }
      .shell { grid-template-columns: 1fr; }
      .sidebar {
        position: static;
        min-height: 460px;
        max-height: 560px;
      }
      .metric-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    }

    @media (max-width: 560px) {
      .masthead { padding: 24px 18px; }
      .shell { padding: 0 10px; }
      .content { padding: 19px 14px; }
      .detail-head, .table-tools {
        align-items: stretch;
        flex-direction: column;
      }
      .primary-button { width: 100%; }
      .tool-fields { align-items: stretch; flex-direction: column; }
      .table-search { width: 100%; }
    }
  </style>
</head>
<body data-ready="false">
  <header class="masthead">
    <div class="brand-row">
      <div>
        <p class="eyebrow" id="brand-eyebrow">TDX BLOCK ATLAS · OFFLINE</p>
        <h1 id="brand-title">通达信板块图谱</h1>
        <p class="subtitle" id="brand-subtitle">行业、概念、风格与指数的双向关系浏览器。全部数据压缩在此文件中，不联网、不依赖服务器。</p>
      </div>
      <div class="snapshot" id="snapshot">正在读取本地数据…</div>
    </div>
    <div class="stats">
      <div class="stat"><strong id="stat-blocks">—</strong><span>板块</span></div>
      <div class="stat"><strong id="stat-relations">—</strong><span>板块—证券关系</span></div>
      <div class="stat"><strong id="stat-securities">—</strong><span>相关证券</span></div>
      <div class="stat hidden" id="stat-market-box"><strong id="stat-market">—</strong><span id="stat-market-label">行情覆盖</span></div>
      <div class="stat"><strong>__PACKED_SIZE__</strong><span>HTML 内嵌压缩数据</span></div>
    </div>
  </header>

  <main class="shell">
    <aside class="sidebar">
      <div class="side-controls">
        <div class="mode-switch">
          <button id="mode-blocks" class="active" type="button">按板块</button>
          <button id="mode-securities" type="button">按证券</button>
        </div>
        <label class="search-wrap">
          <input id="global-search" type="search" placeholder="搜索板块名、880代码…" autocomplete="off">
        </label>
        <div class="families" id="families"></div>
        <div class="block-view-switch" id="block-view-switch">
          <span>板块导航</span>
          <button id="view-tree" class="active" type="button">层级树</button>
          <button id="view-flat" type="button">平铺</button>
          <button id="view-market" class="hidden" type="button">行情排行</button>
        </div>
      </div>
      <div class="result-summary" id="result-summary">正在建立索引…</div>
      <div class="result-list" id="result-list"></div>
    </aside>

    <section class="content" id="content">
      <div class="loading">
        <div>
          <div class="loader-ring"></div>
          <strong>正在解压板块数据</strong>
          <p>数据只在浏览器内存中处理</p>
        </div>
      </div>
    </section>
  </main>

  <script id="tdx-data" type="application/gzip">__PAYLOAD__</script>
  <script>
  (() => {
    "use strict";

    const bootStarted = performance.now();
    const $ = (id) => document.getElementById(id);
    const number = new Intl.NumberFormat("zh-CN");
    const marketCodes = ["SZ", "SH", "BJ"];
    const marketNames = ["深圳", "上海", "北京"];
    const state = {
      mode: "blocks",
      family: "all",
      blockView: "tree",
      query: "",
      selectedBlock: 0,
      selectedSecurity: 0,
      memberQuery: "",
      sort: "code",
      page: 0,
      pageSize: 200,
      reverse: [],
      children: [],
      roots: [],
      expanded: new Set()
    };
    let db;

    function node(tag, className, text) {
      const item = document.createElement(tag);
      if (className) item.className = className;
      if (text !== undefined) item.textContent = text;
      return item;
    }

    function clear(item) {
      while (item.firstChild) item.removeChild(item.firstChild);
    }

    function familyTag(familyIndex) {
      return node(
        "span",
        `family-tag family-${familyIndex}`,
        db.families[familyIndex][1]
      );
    }

    function relationTag(direct) {
      return node(
        "span",
        `relation-tag ${direct ? "relation-direct" : "relation-descendant"}`,
        direct ? "直接归属" : "子行业聚合"
      );
    }

    function securityQuote(index) {
      return db.market && db.market.security_quotes
        ? db.market.security_quotes[index]
        : null;
    }

    function blockQuote(index) {
      return db.market && db.market.block_quotes
        ? db.market.block_quotes[index]
        : null;
    }

    function blockMetric(index) {
      return db.market && db.market.block_metrics
        ? db.market.block_metrics[index]
        : null;
    }

    function changeClass(value) {
      if (value > 0.0000001) return "change-up";
      if (value < -0.0000001) return "change-down";
      return "change-flat";
    }

    function formatPrice(value) {
      if (!Number.isFinite(Number(value))) return "—";
      const numeric = Number(value);
      return numeric >= 1000
        ? numeric.toLocaleString("zh-CN", { maximumFractionDigits: 2 })
        : numeric.toLocaleString("zh-CN", {
            minimumFractionDigits: 2,
            maximumFractionDigits: 4
          });
    }

    function formatPct(value) {
      if (!Number.isFinite(Number(value))) return "—";
      const numeric = Number(value);
      return `${numeric > 0 ? "+" : ""}${numeric.toFixed(2)}%`;
    }

    function formatAmount(value) {
      const numeric = Number(value);
      if (!Number.isFinite(numeric)) return "—";
      if (Math.abs(numeric) >= 1e8) return `${(numeric / 1e8).toFixed(2)} 亿`;
      if (Math.abs(numeric) >= 1e4) return `${(numeric / 1e4).toFixed(1)} 万`;
      return number.format(Math.round(numeric));
    }

    function quoteBadge(quote) {
      if (!quote) return node("span", "quote-badge change-flat", "—");
      return node(
        "span",
        `quote-badge ${changeClass(Number(quote[10]))}`,
        formatPct(quote[10])
      );
    }

    async function unpack() {
      if (!("DecompressionStream" in window)) {
        throw new Error("当前浏览器不支持原生 gzip 解压，请使用新版 Chrome、Edge、Firefox 或 Safari 直接打开。");
      }
      const encoded = $("tdx-data").textContent.trim();
      const binary = atob(encoded);
      const bytes = new Uint8Array(binary.length);
      for (let i = 0; i < binary.length; i += 1) {
        bytes[i] = binary.charCodeAt(i);
      }
      const stream = new Blob([bytes])
        .stream()
        .pipeThrough(new DecompressionStream("gzip"));
      return JSON.parse(await new Response(stream).text());
    }

    function buildIndexes() {
      state.reverse = db.reverse;
      state.children = Array.from({ length: db.blocks.length }, () => []);
      state.roots = Array.from({ length: db.families.length }, () => []);
      db.blocks.forEach((block, index) => {
        const parent = block[3];
        if (
          parent >= 0 &&
          db.blocks[parent] &&
          db.blocks[parent][0] === block[0]
        ) {
          state.children[parent].push(index);
        } else {
          state.roots[block[0]].push(index);
        }
      });
      state.children.forEach((children) => children.sort(compareBlocks));
      state.roots.forEach((roots) => roots.sort(compareBlocks));

      const industryFamily = db.families.findIndex(
        (family) => family[0] === "industry"
      );
      if (industryFamily >= 0) {
        state.family = String(industryFamily);
      }
      db.families.forEach((family, familyIndex) => {
        if (
          family[0] === "industry" ||
          family[0] === "research-industry"
        ) {
          state.roots[familyIndex].forEach((index) =>
            state.expanded.add(index)
          );
        }
      });
      const defaultRoot = industryFamily >= 0
        ? state.roots[industryFamily][0]
        : 0;
      state.selectedBlock = defaultRoot === undefined ? 0 : defaultRoot;
      expandAncestors(state.selectedBlock);
    }

    function setupHeader() {
      $("stat-blocks").textContent = number.format(db.stats.blocks);
      $("stat-relations").textContent = number.format(db.stats.memberships);
      $("stat-securities").textContent = number.format(db.stats.securities);
      if (db.market) {
        const stats = db.market.stats;
        document.title = "通达信板块雷达";
        $("brand-eyebrow").textContent = "TDX MARKET RADAR · ONE FILE";
        $("brand-title").textContent = "通达信板块雷达";
        $("brand-subtitle").textContent =
          "板块层级、成分股、涨跌广度与领涨拖累已经压缩在此文件中；双击即可离线浏览。";
        $("stat-market-box").classList.remove("hidden");
        $("stat-market").textContent =
          `${number.format(stats.up)} ↑ / ${number.format(stats.down)} ↓`;
        $("stat-market-label").textContent =
          `证券行情 ${number.format(stats.quoted_securities)} · 板块 ${number.format(stats.quoted_blocks)}`;
        $("view-market").classList.remove("hidden");
        $("snapshot").textContent =
          `行情 ${db.market.captured_at} · 本地板块 ${db.source_date || "未知"} · ${db.market.endpoint}`;
      } else {
        $("snapshot").textContent =
          `数据快照 ${db.source_date || "未知"} · 生成于 ${db.generated_at}`;
      }
    }

    function setupFamilies() {
      const box = $("families");
      clear(box);
      const all = node(
        "button",
        state.family === "all" ? "active" : "",
        `全部 ${number.format(db.stats.blocks)}`
      );
      all.type = "button";
      all.dataset.family = "all";
      box.appendChild(all);
      db.families.forEach((family, index) => {
        const button = node(
          "button",
          state.family === String(index) ? "active" : "",
          `${family[1]} ${number.format(family[2])}`
        );
        button.type = "button";
        button.dataset.family = String(index);
        box.appendChild(button);
      });
      box.addEventListener("click", (event) => {
        const button = event.target.closest("button");
        if (!button) return;
        state.family = button.dataset.family;
        for (const child of box.children) {
          child.classList.toggle(
            "active",
            child.dataset.family === state.family
          );
        }
        if (
          state.family !== "all" &&
          db.blocks[state.selectedBlock][0] !== Number(state.family)
        ) {
          const first = state.roots[Number(state.family)][0];
          if (first !== undefined) {
            state.selectedBlock = first;
            expandAncestors(first);
            renderBlock(first);
          }
        }
        renderList();
        scrollToActive();
      });
    }

    function compareBlocks(leftIndex, rightIndex) {
      const left = db.blocks[leftIndex];
      const right = db.blocks[rightIndex];
      return (
        String(left[8]).localeCompare(String(right[8]), "zh-CN", {
          numeric: true
        }) ||
        String(left[2]).localeCompare(String(right[2]), "zh-CN", {
          numeric: true
        })
      );
    }

    function expandAncestors(blockIndex) {
      let current = db.blocks[blockIndex] ? db.blocks[blockIndex][3] : -1;
      while (current >= 0) {
        state.expanded.add(current);
        current = db.blocks[current][3];
      }
    }

    function scrollToActive() {
      const list = $("result-list");
      const active = list.querySelector(".result-item.active");
      if (!active) return;
      const listRect = list.getBoundingClientRect();
      const activeRect = active.getBoundingClientRect();
      if (activeRect.top < listRect.top) {
        list.scrollTop -= listRect.top - activeRect.top;
      } else if (activeRect.bottom > listRect.bottom) {
        list.scrollTop += activeRect.bottom - listRect.bottom;
      }
    }

    function normalized(value) {
      return String(value || "").trim().toLocaleLowerCase("zh-CN");
    }

    function blockMatches(block, query) {
      if (!query) return true;
      const family = db.families[block[0]];
      return normalized(
        `${block[1]} ${block[2]} ${block[8]} ${family[0]} ${family[1]}`
      ).includes(query);
    }

    function securityMatches(security, query) {
      if (!query) return true;
      const market = marketCodes[security[0]] || `M${security[0]}`;
      return normalized(
        `${market}${security[1]} ${security[1]} ${security[2]}`
      ).includes(query);
    }

    function makeBlockResult(index) {
      const block = db.blocks[index];
      const button = node(
        "button",
        `result-item${index === state.selectedBlock ? " active" : ""}`
      );
      button.type = "button";
      button.dataset.index = String(index);
      const title = node("div", "result-title");
      const titleSide = node("span", "result-title-side");
      if (db.market) {
        const liveQuote = blockQuote(index);
        const aggregate = blockMetric(index);
        const displayQuote = liveQuote ||
          (aggregate && aggregate[0]
            ? [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, aggregate[4]]
            : null);
        titleSide.appendChild(quoteBadge(displayQuote));
      }
      titleSide.appendChild(node("span", "count", number.format(block[6])));
      title.append(
        node("span", "", block[1]),
        titleSide
      );
      const meta = node("div", "result-meta");
      meta.append(
        familyTag(block[0]),
        node("span", "code-cell", block[2] || block[8])
      );
      button.append(title, meta);
      return button;
    }

    function collectDescendants(blockIndex, output) {
      for (const child of state.children[blockIndex]) {
        if (output.has(child)) continue;
        output.add(child);
        collectDescendants(child, output);
      }
    }

    function treeSearchState(query) {
      if (!query) return { visible: null, matches: null, count: 0 };
      const matches = new Set();
      db.blocks.forEach((block, index) => {
        if (
          state.family !== "all" &&
          Number(state.family) !== block[0]
        ) return;
        if (blockMatches(block, query)) matches.add(index);
      });
      const visible = new Set(matches);
      for (const index of matches) {
        let parent = db.blocks[index][3];
        while (parent >= 0) {
          visible.add(parent);
          parent = db.blocks[parent][3];
        }
        collectDescendants(index, visible);
      }
      return { visible, matches, count: matches.size };
    }

    function renderTreeNode(
      fragment,
      blockIndex,
      depth,
      visible,
      searching
    ) {
      if (visible && !visible.has(blockIndex)) return;
      const visibleChildren = state.children[blockIndex].filter(
        (child) => !visible || visible.has(child)
      );
      const expanded = searching || state.expanded.has(blockIndex);
      const row = node(
        "div",
        `tree-node${depth === 0 ? " root" : ""}`
      );
      row.style.paddingLeft = `${7 + depth * 15}px`;
      row.setAttribute("role", "treeitem");
      row.setAttribute("aria-level", String(depth + 1));
      if (visibleChildren.length) {
        row.setAttribute("aria-expanded", String(expanded));
      }

      const toggle = node(
        "button",
        `tree-toggle${expanded ? " expanded" : ""}`
      );
      toggle.type = "button";
      toggle.dataset.toggle = String(blockIndex);
      toggle.disabled = visibleChildren.length === 0;
      toggle.setAttribute(
        "aria-label",
        expanded ? "收起子行业" : "展开子行业"
      );
      toggle.setAttribute("aria-expanded", String(expanded));
      toggle.appendChild(node("span", "", "›"));
      row.append(toggle, makeBlockResult(blockIndex));
      fragment.appendChild(row);

      if (visibleChildren.length && expanded) {
        visibleChildren.forEach((child) =>
          renderTreeNode(
            fragment,
            child,
            depth + 1,
            visible,
            searching
          )
        );
      }
    }

    function renderBlockTree(fragment, query) {
      const search = treeSearchState(query);
      const familyIndices = state.family === "all"
        ? db.families.map((_, index) => index)
        : [Number(state.family)];
      let rendered = 0;
      for (const familyIndex of familyIndices) {
        const roots = state.roots[familyIndex].filter(
          (root) => !search.visible || search.visible.has(root)
        );
        if (!roots.length) continue;
        if (state.family === "all") {
          const heading = node("div", "tree-family-heading");
          heading.append(
            node("span", "", db.families[familyIndex][1]),
            node(
              "span",
              "",
              number.format(db.families[familyIndex][2])
            )
          );
          fragment.appendChild(heading);
        }
        roots.forEach((root) => {
          renderTreeNode(
            fragment,
            root,
            0,
            search.visible,
            Boolean(query)
          );
          rendered += 1;
        });
      }
      const total = state.family === "all"
        ? db.stats.blocks
        : db.families[Number(state.family)][2];
      $("result-summary").textContent = query
        ? `匹配 ${number.format(search.count)} 个板块 · 已展开层级路径`
        : `${number.format(total)} 个板块 · 树状导航`;
      return rendered;
    }

    function renderList() {
      const list = $("result-list");
      const query = normalized(state.query);
      clear(list);
      list.removeAttribute("role");
      const fragment = document.createDocumentFragment();

      if (state.mode === "blocks") {
        if (state.blockView === "tree") {
          list.setAttribute("role", "tree");
          renderBlockTree(fragment, query);
          if (!fragment.childNodes.length) {
            fragment.appendChild(node("div", "empty", "没有匹配结果"));
          }
          list.appendChild(fragment);
          return;
        }
        const indices = [];
        db.blocks.forEach((block, index) => {
          if (state.family !== "all" && Number(state.family) !== block[0]) return;
          if (blockMatches(block, query)) indices.push(index);
        });
        if (state.blockView === "market") {
          indices.sort((left, right) => {
            const leftQuote = blockQuote(left);
            const rightQuote = blockQuote(right);
            const leftMetric = blockMetric(left);
            const rightMetric = blockMetric(right);
            const leftValue = leftQuote
              ? Number(leftQuote[10])
              : leftMetric && leftMetric[0] ? Number(leftMetric[4]) : -Infinity;
            const rightValue = rightQuote
              ? Number(rightQuote[10])
              : rightMetric && rightMetric[0] ? Number(rightMetric[4]) : -Infinity;
            return rightValue - leftValue || compareBlocks(left, right);
          });
          $("result-summary").textContent =
            `${number.format(indices.length)} 个板块 · 按指数涨幅/成分均值排序`;
        } else {
          $("result-summary").textContent = `找到 ${number.format(indices.length)} 个板块`;
        }
        for (const index of indices) {
          fragment.appendChild(makeBlockResult(index));
        }
      } else {
        const indices = [];
        db.securities.forEach((security, index) => {
          if (securityMatches(security, query)) indices.push(index);
        });
        $("result-summary").textContent = `找到 ${number.format(indices.length)} 只证券`;
        for (const index of indices) {
          const security = db.securities[index];
          const market = marketCodes[security[0]] || `M${security[0]}`;
          const button = node(
            "button",
            `result-item${index === state.selectedSecurity ? " active" : ""}`
          );
          button.type = "button";
          button.dataset.index = String(index);
          const title = node("div", "result-title");
          const displayName = security[2] || "名称未解析";
          const titleSide = node("span", "result-title-side");
          if (db.market) titleSide.appendChild(quoteBadge(securityQuote(index)));
          titleSide.appendChild(
            node("span", "count", number.format(state.reverse[index].length))
          );
          title.append(
            node("span", security[2] ? "" : "unresolved", displayName),
            titleSide
          );
          const meta = node("div", "result-meta");
          meta.append(
            node("span", "code-cell", `${market}${security[1]}`),
            node("span", "", marketNames[security[0]] || `市场${security[0]}`)
          );
          button.append(title, meta);
          fragment.appendChild(button);
        }
      }

      if (!fragment.childNodes.length) {
        fragment.appendChild(node("div", "empty", "没有匹配结果"));
      }
      list.appendChild(fragment);
    }

    function switchMode(mode) {
      state.mode = mode;
      state.query = "";
      state.page = 0;
      $("global-search").value = "";
      $("mode-blocks").classList.toggle("active", mode === "blocks");
      $("mode-securities").classList.toggle("active", mode === "securities");
      $("families").classList.toggle("hidden", mode !== "blocks");
      $("block-view-switch").classList.toggle("hidden", mode !== "blocks");
      $("global-search").placeholder =
        mode === "blocks"
          ? "搜索板块名、880代码…"
          : "搜索股票名、证券代码…";
      renderList();
      if (mode === "blocks") renderBlock(state.selectedBlock);
      else renderSecurity(state.selectedSecurity);
    }

    function metric(label, value, valueClass = "") {
      const box = node("div", "metric");
      box.append(node("span", "", label), node("strong", valueClass, value));
      return box;
    }

    function clickableChip(label, blockIndex) {
      const button = node("button", "chip-button", label);
      button.type = "button";
      button.addEventListener("click", () => selectBlock(blockIndex));
      return button;
    }

    function blockChain(blockIndex) {
      const chain = [];
      let current = db.blocks[blockIndex][3];
      while (current >= 0) {
        chain.unshift(current);
        current = db.blocks[current][3];
      }
      return chain;
    }

    function childBlocks(blockIndex) {
      const children = [];
      db.blocks.forEach((block, index) => {
        if (block[3] === blockIndex) children.push(index);
      });
      return children;
    }

    function detailHeader(kicker, title, code) {
      const head = node("div", "detail-head");
      const left = node("div");
      const kickerRow = node("div", "detail-kicker");
      kickerRow.appendChild(kicker);
      left.append(
        kickerRow,
        node("h2", "detail-title", title),
        node("p", "detail-code code-cell", code)
      );
      head.appendChild(left);
      return { head, left };
    }

    function selectBlock(blockIndex) {
      state.selectedBlock = blockIndex;
      expandAncestors(blockIndex);
      state.memberQuery = "";
      state.page = 0;
      state.sort = "code";
      if (state.mode !== "blocks") switchMode("blocks");
      else {
        renderList();
        renderBlock(blockIndex);
      }
      scrollToActive();
    }

    function selectSecurity(securityIndex) {
      state.selectedSecurity = securityIndex;
      if (state.mode !== "securities") switchMode("securities");
      else {
        renderList();
        renderSecurity(securityIndex);
      }
      scrollToActive();
    }

    function getVisibleMembers(blockIndex) {
      const query = normalized(state.memberQuery);
      const rows = db.members[blockIndex].map((packed) => ({
        securityIndex: packed >>> 1,
        direct: Boolean(packed & 1)
      }));
      const filtered = rows.filter(({ securityIndex }) =>
        securityMatches(db.securities[securityIndex], query)
      );
      const collator = new Intl.Collator("zh-CN", {
        numeric: true,
        sensitivity: "base"
      });
      filtered.sort((left, right) => {
        const a = db.securities[left.securityIndex];
        const b = db.securities[right.securityIndex];
        if (state.sort === "name") {
          return collator.compare(a[2] || "", b[2] || "");
        }
        if (state.sort === "market") {
          return a[0] - b[0] || collator.compare(a[1], b[1]);
        }
        if (state.sort === "change") {
          const leftQuote = securityQuote(left.securityIndex);
          const rightQuote = securityQuote(right.securityIndex);
          const leftValue = leftQuote ? Number(leftQuote[10]) : -Infinity;
          const rightValue = rightQuote ? Number(rightQuote[10]) : -Infinity;
          return rightValue - leftValue ||
            collator.compare(a[1], b[1]) || a[0] - b[0];
        }
        if (state.sort === "amount") {
          const leftQuote = securityQuote(left.securityIndex);
          const rightQuote = securityQuote(right.securityIndex);
          const leftValue = leftQuote ? Number(leftQuote[6]) : -Infinity;
          const rightValue = rightQuote ? Number(rightQuote[6]) : -Infinity;
          return rightValue - leftValue ||
            collator.compare(a[1], b[1]) || a[0] - b[0];
        }
        return collator.compare(a[1], b[1]) || a[0] - b[0];
      });
      return filtered;
    }

    function makeTable(headers) {
      const wrap = node("div", "table-wrap");
      const table = node("table");
      const thead = node("thead");
      const headRow = node("tr");
      headers.forEach((header) => headRow.appendChild(node("th", "", header)));
      thead.appendChild(headRow);
      const tbody = node("tbody");
      table.append(thead, tbody);
      wrap.appendChild(table);
      return { wrap, tbody };
    }

    function addPager(parent, total, renderPage) {
      const pages = Math.max(1, Math.ceil(total / state.pageSize));
      state.page = Math.min(state.page, pages - 1);
      const pager = node("div", "pager");
      const previous = node("button", "", "上一页");
      const next = node("button", "", "下一页");
      previous.type = next.type = "button";
      previous.disabled = state.page <= 0;
      next.disabled = state.page >= pages - 1;
      previous.addEventListener("click", () => {
        state.page -= 1;
        renderPage();
      });
      next.addEventListener("click", () => {
        state.page += 1;
        renderPage();
      });
      pager.append(
        node("span", "", `第 ${state.page + 1} / ${pages} 页 · ${number.format(total)} 条`),
        previous,
        next
      );
      parent.appendChild(pager);
    }

    function csvCell(value) {
      const text = String(value == null ? "" : value);
      return /[",\r\n]/.test(text)
        ? `"${text.replace(/"/g, '""')}"`
        : text;
    }

    function downloadBlockCsv(blockIndex) {
      const block = db.blocks[blockIndex];
      const family = db.families[block[0]];
      const lines = [
        [
          "family", "family_name", "block_name", "block_code", "market",
          "code", "security_name", "membership", "last_price",
          "change_pct", "amount"
        ]
      ];
      for (const packed of db.members[blockIndex]) {
        const securityIndex = packed >>> 1;
        const security = db.securities[securityIndex];
        const quote = securityQuote(securityIndex);
        lines.push([
          family[0],
          family[1],
          block[1],
          block[2],
          marketCodes[security[0]] || `M${security[0]}`,
          security[1],
          security[2],
          packed & 1 ? "direct" : "descendant",
          quote ? quote[0] : "",
          quote ? quote[10] : "",
          quote ? quote[6] : ""
        ]);
      }
      const csv = "\uFEFF" + lines.map(
        (row) => row.map(csvCell).join(",")
      ).join("\r\n");
      const url = URL.createObjectURL(
        new Blob([csv], { type: "text/csv;charset=utf-8" })
      );
      const link = document.createElement("a");
      link.href = url;
      link.download = `${block[1].replace(/[\\/:*?"<>|]/g, "_")}-${block[2] || block[8]}.csv`;
      document.body.appendChild(link);
      link.click();
      link.remove();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
    }

    function renderBlock(blockIndex) {
      state.selectedBlock = blockIndex;
      const block = db.blocks[blockIndex];
      const family = db.families[block[0]];
      const content = $("content");
      clear(content);

      const { head } = detailHeader(
        familyTag(block[0]),
        block[1],
        block[2] || block[8]
      );
      const exportButton = node("button", "primary-button", "导出当前板块 CSV");
      exportButton.type = "button";
      exportButton.addEventListener("click", () => downloadBlockCsv(blockIndex));
      head.appendChild(exportButton);
      content.appendChild(head);

      const metrics = node("div", "metric-grid");
      const liveQuote = blockQuote(blockIndex);
      const liveMetric = blockMetric(blockIndex);
      if (db.market && liveMetric) {
        const leaderIndex = Number(liveMetric[5]);
        const laggardIndex = Number(liveMetric[7]);
        const leaderName = leaderIndex >= 0
          ? (db.securities[leaderIndex][2] || db.securities[leaderIndex][1])
          : "—";
        const laggardName = laggardIndex >= 0
          ? (db.securities[laggardIndex][2] || db.securities[laggardIndex][1])
          : "—";
        metrics.append(
          metric("板块指数", liveQuote ? formatPrice(liveQuote[0]) : "—"),
          metric(
            "指数涨跌",
            liveQuote ? formatPct(liveQuote[10]) : "—",
            liveQuote ? changeClass(Number(liveQuote[10])) : ""
          ),
          metric(
            "上涨 / 下跌 / 平盘",
            `${number.format(liveMetric[1])} / ${number.format(liveMetric[2])} / ${number.format(liveMetric[3])}`
          ),
          metric(
            "成分平均涨跌",
            liveMetric[0] ? formatPct(liveMetric[4]) : "—",
            liveMetric[0] ? changeClass(Number(liveMetric[4])) : ""
          ),
          metric(
            "领涨",
            leaderIndex >= 0
              ? `${leaderName} ${formatPct(liveMetric[6])}`
              : "—",
            leaderIndex >= 0 ? changeClass(Number(liveMetric[6])) : ""
          ),
          metric(
            "拖累",
            laggardIndex >= 0
              ? `${laggardName} ${formatPct(liveMetric[8])}`
              : "—",
            laggardIndex >= 0 ? changeClass(Number(liveMetric[8])) : ""
          ),
          metric("成分成交额", formatAmount(liveMetric[9])),
          metric(
            "行情覆盖",
            `${number.format(liveMetric[0])} / ${number.format(block[6])}`
          )
        );
      } else {
        metrics.append(
          metric("成员证券", number.format(block[6])),
          metric("行业层级", block[4] ? `第 ${block[4]} 级` : "—"),
          metric("源代码", block[8] || "—"),
          metric("更新日期", block[7] || db.source_date || "—")
        );
      }
      content.appendChild(metrics);

      const chain = blockChain(blockIndex);
      const children = childBlocks(blockIndex);
      if (chain.length || children.length) {
        const relations = node("div", "relations");
        if (chain.length) {
          const row = node("div", "relation-row");
          row.appendChild(node("span", "relation-label", "上级"));
          const chips = node("div", "chip-group");
          chain.forEach((index) =>
            chips.appendChild(clickableChip(db.blocks[index][1], index))
          );
          row.appendChild(chips);
          relations.appendChild(row);
        }
        if (children.length) {
          const row = node("div", "relation-row");
          row.appendChild(node("span", "relation-label", "下级"));
          const chips = node("div", "chip-group");
          children.forEach((index) =>
            chips.appendChild(clickableChip(db.blocks[index][1], index))
          );
          row.appendChild(chips);
          relations.appendChild(row);
        }
        content.appendChild(relations);
      }

      const tools = node("div", "table-tools");
      tools.appendChild(node("h3", "", "成分证券"));
      const fields = node("div", "tool-fields");
      const search = node("input", "table-search");
      search.type = "search";
      search.placeholder = "在当前板块搜索股票…";
      search.value = state.memberQuery;
      const sort = node("select");
      [
        ["code", "按证券代码"],
        ["name", "按证券名称"],
        ["market", "按市场"],
        ...(db.market ? [
          ["change", "按涨跌幅"],
          ["amount", "按成交额"]
        ] : [])
      ].forEach(([value, label]) => {
        const option = node("option", "", label);
        option.value = value;
        option.selected = state.sort === value;
        sort.appendChild(option);
      });
      fields.append(search, sort);
      tools.appendChild(fields);
      content.appendChild(tools);

      const tableHost = node("div");
      content.appendChild(tableHost);

      const renderMemberTable = () => {
        clear(tableHost);
        const rows = getVisibleMembers(blockIndex);
        const headers = ["市场", "代码", "证券名称"];
        if (db.market) headers.push("现价", "涨跌幅", "成交额");
        headers.push("归属");
        const { wrap, tbody } = makeTable(headers);
        const start = state.page * state.pageSize;
        rows.slice(start, start + state.pageSize).forEach(({ securityIndex, direct }) => {
          const security = db.securities[securityIndex];
          const market = marketCodes[security[0]] || `M${security[0]}`;
          const quote = securityQuote(securityIndex);
          const tr = node("tr");
          tr.append(
            node("td", "", marketNames[security[0]] || `市场${security[0]}`),
            node("td", "code-cell", `${market}${security[1]}`),
            node("td", security[2] ? "" : "unresolved", security[2] || "名称未解析")
          );
          if (db.market) {
            tr.append(
              node("td", "numeric", quote ? formatPrice(quote[0]) : "—"),
              node(
                "td",
                `numeric ${quote ? changeClass(Number(quote[10])) : ""}`,
                quote ? formatPct(quote[10]) : "—"
              ),
              node("td", "numeric", quote ? formatAmount(quote[6]) : "—")
            );
          }
          const relation = node("td");
          relation.appendChild(relationTag(direct));
          tr.appendChild(relation);
          tr.addEventListener("click", () => selectSecurity(securityIndex));
          tbody.appendChild(tr);
        });
        if (!rows.length) {
          const tr = node("tr");
          const td = node("td", "empty", "当前板块没有匹配证券");
          td.colSpan = db.market ? 7 : 4;
          tr.appendChild(td);
          tbody.appendChild(tr);
        }
        tableHost.appendChild(wrap);
        addPager(tableHost, rows.length, renderMemberTable);
      };

      let searchTimer;
      search.addEventListener("input", () => {
        clearTimeout(searchTimer);
        searchTimer = setTimeout(() => {
          state.memberQuery = search.value;
          state.page = 0;
          renderMemberTable();
        }, 80);
      });
      sort.addEventListener("change", () => {
        state.sort = sort.value;
        state.page = 0;
        renderMemberTable();
      });
      renderMemberTable();

      content.appendChild(
        node(
          "p",
          "footnote",
          "direct 表示证券直接归属该行业叶子或板块；descendant 表示因子行业归属而被聚合到父行业。点击证券可反查它所属的全部板块。"
        )
      );
    }

    function renderSecurity(securityIndex) {
      state.selectedSecurity = securityIndex;
      const security = db.securities[securityIndex];
      const market = marketCodes[security[0]] || `M${security[0]}`;
      const content = $("content");
      clear(content);

      const tag = node("span", "family-tag family-4", marketNames[security[0]] || `市场${security[0]}`);
      const { head } = detailHeader(
        tag,
        security[2] || "名称未解析",
        `${market}${security[1]}`
      );
      content.appendChild(head);

      const metrics = node("div", "metric-grid");
      const liveQuote = securityQuote(securityIndex);
      if (db.market) {
        metrics.append(
          metric("现价", liveQuote ? formatPrice(liveQuote[0]) : "—"),
          metric(
            "涨跌幅",
            liveQuote ? formatPct(liveQuote[10]) : "—",
            liveQuote ? changeClass(Number(liveQuote[10])) : ""
          ),
          metric("今开", liveQuote ? formatPrice(liveQuote[2]) : "—"),
          metric(
            "最高 / 最低",
            liveQuote
              ? `${formatPrice(liveQuote[3])} / ${formatPrice(liveQuote[4])}`
              : "—"
          ),
          metric("成交额", liveQuote ? formatAmount(liveQuote[6]) : "—"),
          metric("成交量（手）", liveQuote ? number.format(liveQuote[5]) : "—"),
          metric("所属板块", number.format(state.reverse[securityIndex].length)),
          metric("市场", marketNames[security[0]] || `市场${security[0]}`)
        );
      } else {
        metrics.append(
          metric("所属板块", number.format(state.reverse[securityIndex].length)),
          metric("市场", marketNames[security[0]] || `市场${security[0]}`),
          metric("证券代码", security[1]),
          metric("名称状态", security[2] ? "已解析" : "仅保留代码")
        );
      }
      content.appendChild(metrics);
      const tools = node("div", "table-tools");
      tools.appendChild(node("h3", "", "所属板块"));
      content.appendChild(tools);

      const headers = ["类别", "板块名称", "板块代码"];
      if (db.market) headers.push("板块涨跌");
      headers.push("归属");
      const { wrap, tbody } = makeTable(headers);
      const memberships = [...state.reverse[securityIndex]].sort((a, b) => {
        const left = db.blocks[a >>> 1];
        const right = db.blocks[b >>> 1];
        return left[0] - right[0] || left[1].localeCompare(right[1], "zh-CN");
      });
      for (const packed of memberships) {
        const blockIndex = packed >>> 1;
        const direct = Boolean(packed & 1);
        const block = db.blocks[blockIndex];
        const tr = node("tr");
        const familyCell = node("td");
        familyCell.appendChild(familyTag(block[0]));
        tr.append(
          familyCell,
          node("td", "", block[1]),
          node("td", "code-cell", block[2] || block[8])
        );
        if (db.market) {
          const quote = blockQuote(blockIndex);
          const aggregate = blockMetric(blockIndex);
          const value = quote
            ? Number(quote[10])
            : aggregate && aggregate[0] ? Number(aggregate[4]) : null;
          tr.appendChild(
            node(
              "td",
              `numeric ${value === null ? "" : changeClass(value)}`,
              value === null ? "—" : formatPct(value)
            )
          );
        }
        const relation = node("td");
        relation.appendChild(relationTag(direct));
        tr.appendChild(relation);
        tr.addEventListener("click", () => selectBlock(blockIndex));
        tbody.appendChild(tr);
      }
      if (!memberships.length) {
        const tr = node("tr");
        const td = node("td", "empty", "没有板块关系");
        td.colSpan = db.market ? 5 : 4;
        tr.appendChild(td);
        tbody.appendChild(tr);
      }
      content.appendChild(wrap);
      content.appendChild(
        node("p", "footnote", "点击任意板块可切回其完整成分证券列表。")
      );
    }

    function bindEvents() {
      $("mode-blocks").addEventListener("click", () => switchMode("blocks"));
      $("mode-securities").addEventListener("click", () => switchMode("securities"));
      $("view-tree").addEventListener("click", () => {
        state.blockView = "tree";
        $("view-tree").classList.add("active");
        $("view-flat").classList.remove("active");
        $("view-market").classList.remove("active");
        renderList();
        scrollToActive();
      });
      $("view-flat").addEventListener("click", () => {
        state.blockView = "flat";
        $("view-flat").classList.add("active");
        $("view-tree").classList.remove("active");
        $("view-market").classList.remove("active");
        renderList();
        scrollToActive();
      });
      $("view-market").addEventListener("click", () => {
        state.blockView = "market";
        $("view-market").classList.add("active");
        $("view-tree").classList.remove("active");
        $("view-flat").classList.remove("active");
        renderList();
        scrollToActive();
      });
      let searchFrame;
      $("global-search").addEventListener("input", (event) => {
        cancelAnimationFrame(searchFrame);
        searchFrame = requestAnimationFrame(() => {
          state.query = event.target.value;
          renderList();
        });
      });
      $("result-list").addEventListener("click", (event) => {
        const toggle = event.target.closest(".tree-toggle");
        if (toggle) {
          const index = Number(toggle.dataset.toggle);
          if (state.expanded.has(index)) state.expanded.delete(index);
          else state.expanded.add(index);
          renderList();
          return;
        }
        const item = event.target.closest(".result-item");
        if (!item) return;
        const index = Number(item.dataset.index);
        if (state.mode === "blocks") selectBlock(index);
        else selectSecurity(index);
      });
    }

    async function start() {
      try {
        db = await unpack();
        if (db.v !== 1) throw new Error(`不支持的数据版本：${db.v}`);
        buildIndexes();
        setupHeader();
        setupFamilies();
        bindEvents();
        renderList();
        renderBlock(state.selectedBlock);
        scrollToActive();
        const loadMs = Math.round(performance.now() - bootStarted);
        document.body.dataset.loadMs = String(loadMs);
        $("snapshot").textContent += ` · 载入 ${loadMs} ms`;
        document.body.dataset.ready = "true";
      } catch (error) {
        const content = $("content");
        clear(content);
        const message = node("div", "empty");
        message.append(
          node("h2", "", "页面数据加载失败"),
          node("p", "", error && error.message ? error.message : String(error))
        );
        content.appendChild(message);
        $("result-summary").textContent = "未能建立索引";
        document.body.dataset.loadMs = String(
          Math.round(performance.now() - bootStarted)
        );
        document.body.dataset.ready = "error";
      }
    }

    start();
  })();
  </script>
</body>
</html>
"""


def compact_payload(
    blocks: list[extractor.Block],
    members: list[extractor.BlockMember],
    generated_at: str | None = None,
) -> dict[str, object]:
    block_indices = {block.block_id: index for index, block in enumerate(blocks)}
    if len(block_indices) != len(blocks):
        raise extractor.BlockFormatError("duplicate block_id in HTML input")

    member_security = {member.security_id: member for member in members}
    security_ids = sorted(member_security)
    security_indices = {
        security_id: index for index, security_id in enumerate(security_ids)
    }
    families = [
        [
            family,
            extractor.FAMILY_NAMES[family],
            sum(block.family == family for block in blocks),
            sum(member.family == family for member in members),
        ]
        for family in extractor.FAMILY_CHOICES
        if any(block.family == family for block in blocks)
    ]
    family_indices = {
        family[0]: index for index, family in enumerate(families)
    }

    compact_blocks = [
        [
            family_indices[block.family],
            block.name,
            block.block_code,
            block_indices.get(block.parent_block_id, -1),
            block.level,
            int(block.is_leaf),
            block.member_count,
            block.update_date,
            block.source_key,
        ]
        for block in blocks
    ]
    compact_securities = [
        [
            member_security[security_id].market_id,
            member_security[security_id].code,
            member_security[security_id].security_name,
        ]
        for security_id in security_ids
    ]
    compact_members: list[list[int]] = [[] for _ in blocks]
    compact_reverse: list[list[int]] = [[] for _ in security_ids]
    seen_relations: set[tuple[int, int]] = set()
    for member in members:
        block_index = block_indices[member.block_id]
        security_index = security_indices[member.security_id]
        relation = (block_index, security_index)
        if relation in seen_relations:
            raise extractor.BlockFormatError(
                f"duplicate block/security relation: "
                f"{member.block_id}/{member.security_id}"
            )
        seen_relations.add(relation)
        direct = int(member.membership == "direct")
        compact_members[block_index].append((security_index << 1) | direct)
        compact_reverse[security_index].append((block_index << 1) | direct)

    for index, (block, packed_members) in enumerate(
        zip(blocks, compact_members)
    ):
        if block.member_count != len(packed_members):
            raise extractor.BlockFormatError(
                f"{block.block_id}: member_count={block.member_count}, "
                f"packed={len(packed_members)}"
            )
        packed_members.sort()
    for memberships in compact_reverse:
        memberships.sort()

    source_dates = [
        date
        for block in blocks
        for date in (block.update_date, block.start_date)
        if date and len(date) == 8 and date.isdigit()
    ]
    generated_at = generated_at or datetime.now().astimezone().isoformat(
        timespec="seconds"
    )
    return {
        "v": 1,
        "generated_at": generated_at,
        "source_date": max(source_dates, default=""),
        "stats": {
            "blocks": len(blocks),
            "memberships": len(members),
            "securities": len(security_ids),
        },
        "families": families,
        "blocks": compact_blocks,
        "securities": compact_securities,
        "members": compact_members,
        "reverse": compact_reverse,
    }


def encode_payload(document: dict[str, object]) -> tuple[str, int, int]:
    raw = json.dumps(
        document,
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")
    packed = gzip.compress(raw, compresslevel=9, mtime=0)
    return base64.b64encode(packed).decode("ascii"), len(raw), len(packed)


def human_size(size: int) -> str:
    if size < 1024:
        return f"{size} B"
    if size < 1024 * 1024:
        return f"{size / 1024:.1f} KiB"
    return f"{size / (1024 * 1024):.1f} MiB"


def render_html(document: dict[str, object]) -> tuple[str, int, int]:
    encoded, raw_size, packed_size = encode_payload(document)
    html = HTML_TEMPLATE.replace("__PAYLOAD__", encoded).replace(
        "__PACKED_SIZE__", human_size(packed_size)
    )
    return html, raw_size, packed_size


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Read TDX public cache data and build one compressed offline HTML "
            "for block/security bidirectional lookup."
        )
    )
    parser.add_argument("--root", type=Path, required=True, help="TDX install root")
    parser.add_argument(
        "--family",
        action="append",
        choices=extractor.FAMILY_CHOICES,
        help="family to include; repeat as needed (default: all)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="generated single HTML path",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    root = args.root.resolve()
    if not root.is_dir():
        print(f"error: TDX install root is missing: {root}", file=sys.stderr)
        return 2
    families = set(args.family or extractor.FAMILY_CHOICES)
    try:
        _, blocks, members = extractor.extract(root, families)
        document = compact_payload(blocks, members)
        html, raw_size, packed_size = render_html(document)
        output = args.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("w", encoding="utf-8", newline="\n") as stream:
            stream.write(html)
    except (OSError, UnicodeError, extractor.BlockFormatError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    ratio = packed_size / raw_size if raw_size else 0
    print(
        f"built {output}: {len(blocks)} blocks, {len(members)} memberships, "
        f"payload {human_size(raw_size)} -> {human_size(packed_size)} "
        f"({ratio:.1%}), HTML {human_size(output.stat().st_size)}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
