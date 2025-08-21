# 📝 TODO - Potensio CE

---

## 🚀 Phase 1 — MVP (Core Features, ~6 weeks @ 1.5h/day)

### 📂 File Staging
- [X] Base class implementation  
- [X] Interface  
  - [X] Render drag & drop zone  
  - [X] Table & Info  
  - [X] Wiggle to Trigger 
- [X] Drag-and-drop implementation (add dropped file to table)
- [X] Toolbar (minimal)  
  - [X] Paste Here
  - [X] Cut Files
  - [X] Bulk Rename
  - [X] Clear All Staging
  - [X] Move to recycle bin (implemented, but there seem to be bug)
  - [-] Pin Window - PENDING
  - [-] Toggle trigger (keyboard shortcut & wiggle trigger) - PENDING
- [X] Right click file-staging (Context Menu)
  - [X] Remove From Staging
  - [X] Open *NOTE: SLOW!*
  - [X] Open in Explorer *NOTE: SLOW!*
	
### ⏳ Pomodoro
- [X] Base class implementation  
- [X] Basic pomodoro logic  
- [X] Interface implementation  
- [X] DB class implementation  
- [X] Windows native notification (basic cycle/percentage)  
- [ ] Notification management
- [ ] Settings persistence  

### 🗂️ Kanban
- [X] Base class implementation  
- [X] Base interface implementation  
- [X] Base Kanban logic implementation  
- [ ] DB class writing  
- [ ] DB ↔ Kanban data integration  

### ✅ Todo
- [X] Base class implementation  
- [X] Interface implementation  
- [X] Basic Todo logic  
- [ ] DB class writing  
- [ ] DB ↔ Todo data integration  

### ⚙️ Settings
- [X] Base class implementation  
- [X] Interface implementation  
- [ ] General  
- [ ] Appearance  

### 🧰 Toolbar (Essential)
- [ ] File → Exit  
- [ ] Tools → Settings  
- [ ] Tools → Open Log Folder

### 🛠️ Tooling & CI
- [ ] Disable temporary pending task
- [ ] Set up GitHub Actions / GitLab CI  
- [ ] Code signing  via SignPath

---

## 📦 Phase 2 — MVP+ (Productivity Expansion, ~6 weeks)

### 📋 Clipboard Manager
- [X] Base class implementation  
- [X] Interface implementation  
- [X] Basic clipboard logic  
- [ ] Change to per-day clipboard data  
- [ ] DB class writing  
- [ ] DB ↔ Clipboard integration  

### 🖼️ File Converter & Compressor
- [X] Base class implementation  
- [X] Interface implementation  
- [ ] PDF compressor  
- [ ] Image converter (PDF ↔ JPG)  
- [ ] Image compressor  

### ⚙️ Settings (Advanced)
- [ ] Hotkeys  
- [ ] Modules  
- [ ] Account  
- [ ] About  

### 🧰 Toolbar (Extended)
- [ ] File → Export (Kanban & Pomodoro statistics)  
- [ ] Window → Always on Top / Minimize to Tray  
- [ ] Tools → Open Log Folder  
- [ ] Help → User Guide, Keyboard Shortcuts, About Potensio  

---

## 🎨 Phase 3 — Polishing & Distribution (~1–2 months)

### 📖 Documentation
- [ ] Add `README.md` with usage/build instructions  
- [ ] Add `CONTRIBUTING.md` for contributors  
- [ ] Generate Doxygen docs  

### 🚀 Packaging & Cross-Platform
- [ ] Cross-platform support (Linux / macOS)  
- [ ] Package project as `.deb`, `.rpm`, or `.msi`  

---

✨ **Timeline Recap**  
- **Phase 1 (MVP):** ~6 weeks → Core features usable.  
- **Phase 2 (MVP+):** ~6 weeks → Extra productivity modules.  
- **Phase 3 (Polish):** ~1–2 months → CI, docs, packaging, cross-platform.  

Total: **~4–5 months** to reach polished cross-platform release (with 1–2 hrs/day).  
