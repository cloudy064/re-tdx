# tdx-tool Svelte 前端

开发环境需要 Node.js 18 或更高版本；最终发布包运行时不需要 Node.js。

```powershell
npm install
npm run check
npm run dev
```

开发服务器会把 `/api` 代理到 `http://127.0.0.1:8765`。生产构建：

```powershell
npm run build
..\dist\tdx-tool\bin\tdx-tool.exe serve --root C:\new_tdx `
  --jsn-root ..\output\tdx-jsn
```

C++ 服务会自动发现 `web/dist`，安装后的程序则从
`share/tdx-tool/web` 读取同一套构建产物。
