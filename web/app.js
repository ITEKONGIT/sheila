const state = {items: [], tasks: [], recycle: {items: [], tasks: []}, filter: "all", subfilter: "all", query: "", editingNoteId: null, editingTaskId: null, editorMode: "write", dirty: false};
const el = {
  items: document.querySelector("#items"), search: document.querySelector("#search"),
  connection: document.querySelector("#connection"), storage: document.querySelector("#storage"),
  fileInput: document.querySelector("#file-input"), dropZone: document.querySelector("#drop-zone"),
  progress: document.querySelector("#upload-progress"), dialog: document.querySelector("#note-dialog"),
  noteForm: document.querySelector("#note-form"), noteTitle: document.querySelector("#note-title"),
  noteContent: document.querySelector("#note-content"), dialogTitle: document.querySelector("#note-dialog-title"),
  notePreview: document.querySelector("#note-preview"), editorStatus: document.querySelector("#editor-status"),
  wordCount: document.querySelector("#word-count"), saveNote: document.querySelector("#save-note"),
  toast: document.querySelector("#toast"), tasksPanel: document.querySelector("#tasks-panel"), recyclePanel: document.querySelector("#recycle-panel"),
  taskDialog: document.querySelector("#task-dialog"), taskForm: document.querySelector("#task-form"), taskTitle: document.querySelector("#task-title"), taskDetails: document.querySelector("#task-details"), taskPriority: document.querySelector("#task-priority"), taskDue: document.querySelector("#task-due"), taskReminder: document.querySelector("#task-reminder"), taskDialogTitle: document.querySelector("#task-dialog-title"), saveTask: document.querySelector("#save-task"),
  subFilters: document.querySelector("#sub-filters"),
  infoDialog: document.querySelector("#info-dialog"), infoTitle: document.querySelector("#info-title"),
  infoType: document.querySelector("#info-type"), infoSize: document.querySelector("#info-size"),
  infoMime: document.querySelector("#info-mime"), infoChecksum: document.querySelector("#info-checksum"),
  infoCreated: document.querySelector("#info-created"), infoUpdated: document.querySelector("#info-updated"),
  infoPreview: document.querySelector("#info-preview"), infoDownload: document.querySelector("#info-download")
};

// Recycle bin is a dedicated page, not an inline inbox filter. Register this
// before the general navigation handler so it cannot fall through to the old
// panel renderer.
document.querySelector('[data-filter="recycle"]').addEventListener("click", event => {
  event.preventDefault();
  event.stopImmediatePropagation();
  window.location.assign("/recycle-bin");
});

function formatBytes(bytes) { const units = ["B", "KB", "MB", "GB", "TB"]; let value = Number(bytes || 0), unit = 0; while (value >= 1024 && unit < units.length - 1) { value /= 1024; unit++; } return `${value.toFixed(unit < 2 ? 0 : 1)} ${units[unit]}`; }
function formatDate(value) { if (!value) return "Now"; const normalized = value.includes("T") ? value : `${value.replace(" ", "T")}Z`; return new Intl.DateTimeFormat(undefined, {month: "short", day: "numeric", hour: "numeric", minute: "2-digit"}).format(new Date(normalized)); }
function toast(message, isError = false) { el.toast.innerHTML = ""; const text = document.createTextNode(message); el.toast.appendChild(text); el.toast.classList.toggle("error", isError); el.toast.classList.add("show"); clearTimeout(toast.timer); toast.timer = setTimeout(() => el.toast.classList.remove("show"), 2800); }
async function api(url, options = {}) { const response = await fetch(url, options); const payload = await response.json().catch(() => ({})); if (!response.ok) throw new Error(payload.message || `Request failed (${response.status})`); return payload; }

function icon(item) {
  if (item.type === "note") return "TXT";
  const typeMap = {image: "IMG", video: "VID", audio: "AUD", document: "DOC", file: "FILE"};
  if (typeMap[item.type]) return typeMap[item.type];
  const extension = item.title.split(".").pop();
  return extension && extension !== item.title ? extension.slice(0, 4) : "FILE";
}

function visibleItems() {
  return state.items.filter(item => {
    if (state.filter === "note") return item.type === "note";
    if (state.subfilter !== "all") return item.type === state.subfilter;
    if (state.filter === "all") return true;
    return item.type !== "note";
  });
}

function render() {
  const items = visibleItems(), notes = state.items.filter(item => item.type === "note").length, files = state.items.filter(item => item.type !== "note").length;
  document.querySelector("#all-count").textContent = state.items.length; document.querySelector("#file-count").textContent = files; document.querySelector("#note-count").textContent = notes;
  document.querySelector("#item-summary").textContent = `${items.length} item${items.length === 1 ? "" : "s"}`; el.items.replaceChildren();
  if (!items.length) { const empty = document.createElement("div"), title = document.createElement("strong"), detail = document.createElement("span"); empty.className = "empty"; title.textContent = state.query ? "Nothing matched that search" : "This space is ready"; detail.textContent = state.query ? "Try another word or clear the search." : "Send a file or write your first note."; empty.append(title, detail); el.items.append(empty); return; }
  for (const item of items) {
    const row = document.createElement("article"), badge = document.createElement("span"), body = document.createElement("span"), title = document.createElement("span"), preview = document.createElement("span"), meta = document.createElement("span"), del = document.createElement("button");
    row.className = "item"; row.dataset.id = item.id; row.tabIndex = 0; badge.className = "item-icon"; if (["image", "video", "audio", "document"].includes(item.type)) badge.classList.add(`type-${item.type}`); badge.textContent = icon(item); title.className = "item-title"; title.textContent = item.title; preview.className = "item-preview"; preview.textContent = item.type === "note" ? (item.content || "Empty note") : `${item.mediaType || "File"} · ${formatBytes(item.byteSize)}`; body.append(title, preview); meta.className = "item-meta"; meta.textContent = `${formatDate(item.updatedAt)}\n${item.type === "note" ? "Open note" : "Download"}`; meta.style.whiteSpace = "pre-line"; del.type = "button"; del.className = "item-delete"; del.textContent = "\u00d7"; del.dataset.deleteId = item.id; del.setAttribute("aria-label", "Delete"); row.append(badge, body, meta, del);
    if (item.type === "video" && item.downloadUrl) { const vid = document.createElement("video"); vid.className = "item-media"; vid.src = item.downloadUrl; vid.controls = true; vid.preload = "metadata"; vid.playsInline = true; body.append(vid); }
    if (item.type === "audio" && item.downloadUrl) { const aud = document.createElement("audio"); aud.className = "item-media"; aud.src = item.downloadUrl; aud.controls = true; aud.preload = "metadata"; body.append(aud); }
    if (item.type === "image" && item.downloadUrl) { const img = document.createElement("img"); img.className = "item-media"; img.src = item.downloadUrl; img.alt = item.title; img.loading = "lazy"; body.append(img); }
    el.items.append(row);
  }
}

function isoFromInput(value) { return value ? new Date(value).toISOString() : null; }
function inputFromIso(value) { if (!value) return ""; const date = new Date(value.includes("T") ? value : `${value.replace(" ", "T")}Z`); if (Number.isNaN(date.valueOf())) return ""; const pad = n => String(n).padStart(2, "0"); return `${date.getFullYear()}-${pad(date.getMonth()+1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`; }
function taskRow(task, deleted = false) {
  const row = document.createElement("article"); row.className = `task-row priority-${task.priority || 0}${task.status === "completed" ? " completed" : ""}`;
  const body = document.createElement("div"); body.className = "task-body"; const title = document.createElement("strong"); title.textContent = task.title; const details = document.createElement("p"); details.textContent = task.details || ""; body.append(title, details);
  const meta = document.createElement("small"); meta.textContent = task.reminderAt ? `Reminder ${formatDate(task.reminderAt)}` : (task.dueAt ? `Due ${formatDate(task.dueAt)}` : "No date"); body.append(meta);
  const actions = document.createElement("div"); actions.className = "task-actions";
  if (deleted) { const restore = document.createElement("button"); restore.className = "button secondary"; restore.textContent = "Restore"; restore.onclick = () => restoreRecycle("tasks", task.id); const purge = document.createElement("button"); purge.className = "button danger"; purge.textContent = "Delete forever"; purge.onclick = () => purgeRecycle("tasks", task.id); actions.append(restore, purge); }
  else { if (task.status !== "completed") { const done = document.createElement("button"); done.className = "button secondary"; done.textContent = "Complete"; done.onclick = () => completeTask(task.id); actions.append(done); } const edit = document.createElement("button"); edit.className = "button secondary"; edit.textContent = "Edit"; edit.onclick = () => openTask(task); const trash = document.createElement("button"); trash.className = "button danger"; trash.textContent = "Trash"; trash.onclick = () => trashTask(task.id); actions.append(edit, trash); }
  row.append(body, actions); return row;
}
function renderTasks() { el.tasksPanel.replaceChildren(); const head = document.createElement("div"); head.className = "panel-head"; head.innerHTML = `<div><p class="eyebrow">FOLLOW-UPS</p><h2>Tasks & reminders</h2></div><span class="item-summary">${state.tasks.length} active</span>`; el.tasksPanel.append(head); if (!state.tasks.length) { const empty = document.createElement("div"); empty.className = "empty"; empty.textContent = "No tasks yet. Add one when something needs doing."; el.tasksPanel.append(empty); return; } state.tasks.forEach(task => el.tasksPanel.append(taskRow(task))); }
function renderRecycle() { el.recyclePanel.replaceChildren(); const all = [...state.recycle.items, ...state.recycle.tasks]; const head = document.createElement("div"); head.className = "panel-head"; head.innerHTML = `<div><p class="eyebrow">RECOVERABLE</p><h2>Recycle bin</h2></div><span class="item-summary">${all.length} item${all.length === 1 ? "" : "s"}</span>`; el.recyclePanel.append(head); if (!all.length) { const empty = document.createElement("div"); empty.className = "empty"; empty.textContent = "The recycle bin is empty."; el.recyclePanel.append(empty); return; } state.recycle.items.forEach(item => { const row = document.createElement("article"); row.className = "task-row"; const body = document.createElement("div"); body.className = "task-body"; const title = document.createElement("strong"); title.textContent = item.title || "Untitled"; const meta = document.createElement("small"); meta.textContent = `${item.type} · ${formatBytes(item.byteSize)}`; body.append(title, meta); const actions = document.createElement("div"); actions.className = "task-actions"; const restore = document.createElement("button"); restore.className = "button secondary"; restore.textContent = "Restore"; restore.onclick = () => restoreRecycle("items", item.id); const purge = document.createElement("button"); purge.className = "button danger"; purge.textContent = "Delete forever"; purge.onclick = () => purgeRecycle("items", item.id); actions.append(restore, purge); row.append(body, actions); el.recyclePanel.append(row); }); state.recycle.tasks.forEach(task => el.recyclePanel.append(taskRow(task, true))); }

async function loadItems() { try { const suffix = state.query ? `?q=${encodeURIComponent(state.query)}` : ""; const result = await api(`/api/v1/items${suffix}`); state.items = result.items || []; render(); } catch (error) { el.items.replaceChildren(); const box = document.createElement("div"); box.className = "empty"; box.textContent = `Could not load the inbox: ${error.message}`; el.items.append(box); } }
async function loadTasks() { try { const result = await api("/api/v1/tasks"); state.tasks = result.tasks || []; document.querySelector("#task-count").textContent = state.tasks.filter(task => task.status !== "completed").length; renderTasks(); } catch (error) { toast(`Could not load tasks: ${error.message}`, true); } }
async function loadRecycle() { try { state.recycle = await api("/api/v1/recycle-bin"); document.querySelector("#recycle-count").textContent = (state.recycle.items || []).length + (state.recycle.tasks || []).length; renderRecycle(); } catch (error) { toast(`Could not load recycle bin: ${error.message}`, true); } }
function openTask(task = null) { state.editingTaskId = task?.id || null; el.taskDialogTitle.textContent = task ? "Edit task" : "New task"; el.taskTitle.value = task?.title || ""; el.taskDetails.value = task?.details || ""; el.taskPriority.value = String(task?.priority || 0); el.taskDue.value = inputFromIso(task?.dueAt); el.taskReminder.value = inputFromIso(task?.reminderAt); el.taskDialog.showModal(); setTimeout(() => el.taskTitle.focus(), 0); }
async function saveTask(event) { event.preventDefault(); const payload = JSON.stringify({title: el.taskTitle.value.trim(), details: el.taskDetails.value, priority: Number(el.taskPriority.value), dueAt: isoFromInput(el.taskDue.value), reminderAt: isoFromInput(el.taskReminder.value)}); el.saveTask.disabled = true; try { await api(state.editingTaskId ? `/api/v1/tasks/${state.editingTaskId}` : "/api/v1/tasks", {method: state.editingTaskId ? "PUT" : "POST", headers: {"Content-Type": "application/json"}, body: payload}); el.taskDialog.close(); toast(state.editingTaskId ? "Task updated" : "Task created"); await loadTasks(); } catch (error) { toast(error.message, true); } finally { el.saveTask.disabled = false; } }
async function completeTask(id) { try { await api(`/api/v1/tasks/${id}/complete`, {method: "POST"}); await loadTasks(); } catch (error) { toast(error.message, true); } }
async function trashTask(id) { try { await api(`/api/v1/tasks/${id}`, {method: "DELETE"}); toast("Task moved to recycle bin"); await Promise.all([loadTasks(), loadRecycle()]); } catch (error) { toast(error.message, true); } }
async function trashItem(id) { try { await api(`/api/v1/items/${id}`, {method: "DELETE"}); toast("Moved to recycle bin"); await Promise.all([loadItems(), loadRecycle()]); } catch (error) { toast(error.message, true); } }
async function restoreRecycle(kind, id) { try { await api(`/api/v1/recycle-bin/${kind}/${id}/restore`, {method: "POST"}); toast("Restored"); await Promise.all([loadItems(), loadTasks(), loadRecycle()]); } catch (error) { toast(error.message, true); } }
async function purgeRecycle(kind, id) { if (!window.confirm("Delete permanently? This cannot be undone.")) return; try { await api(`/api/v1/recycle-bin/${kind}/${id}`, {method: "DELETE"}); toast("Deleted permanently"); await loadRecycle(); } catch (error) { toast(error.message, true); } }

function draftKey() { return `ssheila.note.draft:${state.editingNoteId || "new"}`; }
function readDraft() { try { const value = localStorage.getItem(draftKey()); return value ? JSON.parse(value) : null; } catch { return null; } }
function clearDraft() { try { localStorage.removeItem(draftKey()); } catch {} }
function saveDraft() { try { localStorage.setItem(draftKey(), JSON.stringify({title: el.noteTitle.value, content: el.noteContent.value, savedAt: new Date().toISOString()})); el.editorStatus.textContent = "Local draft saved"; } catch { el.editorStatus.textContent = "Draft storage unavailable"; } }
function updateEditorStatus() { const text = el.noteContent.value.trim(); const words = text ? text.split(/\s+/).length : 0; el.wordCount.textContent = `${words} word${words === 1 ? "" : "s"} · ${el.noteContent.value.length} characters`; if (state.dirty && el.editorStatus.textContent !== "Local draft saved") el.editorStatus.textContent = "Unsaved changes"; }
function escapeHtml(value) { return value.replace(/[&<>"']/g, character => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"}[character])); }
function inlineMarkdown(value) { return escapeHtml(value).replace(/`([^`]+)`/g, "<code>$1</code>").replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>").replace(/__([^_]+)__/g, "<strong>$1</strong>").replace(/\*([^*]+)\*/g, "<em>$1</em>").replace(/_([^_]+)_/g, "<em>$1</em>"); }
function markdownToHtml(markdown) {
  const output = [], lines = markdown.split(/\r?\n/); let listOpen = false;
  const closeList = () => { if (listOpen) { output.push("</ul>"); listOpen = false; } };
  for (const line of lines) {
    if (/^\s*[-*]\s+/.test(line)) { if (!listOpen) { output.push("<ul>"); listOpen = true; } output.push(`<li>${inlineMarkdown(line.replace(/^\s*[-*]\s+/, ""))}</li>`); continue; }
    closeList();
    if (/^###\s+/.test(line)) output.push(`<h3>${inlineMarkdown(line.replace(/^###\s+/, ""))}</h3>`);
    else if (/^##\s+/.test(line)) output.push(`<h2>${inlineMarkdown(line.replace(/^##\s+/, ""))}</h2>`);
    else if (/^#\s+/.test(line)) output.push(`<h1>${inlineMarkdown(line.replace(/^#\s+/, ""))}</h1>`);
    else if (/^>\s?/.test(line)) output.push(`<blockquote>${inlineMarkdown(line.replace(/^>\s?/, ""))}</blockquote>`);
    else if (/^---+$/.test(line.trim())) output.push("<hr>");
    else if (line.trim()) output.push(`<p>${inlineMarkdown(line)}</p>`);
  }
  closeList(); return output.join("");
}
function renderPreview() { el.notePreview.innerHTML = markdownToHtml(el.noteContent.value || "Nothing written yet."); }
function setEditorMode(mode) { state.editorMode = mode; document.querySelectorAll("[data-editor-mode]").forEach(tab => { const active = tab.dataset.editorMode === mode; tab.classList.toggle("active", active); tab.setAttribute("aria-selected", active ? "true" : "false"); }); el.noteContent.hidden = mode !== "write"; el.notePreview.hidden = mode !== "preview"; if (mode === "preview") renderPreview(); }

function openNote(item = null) {
  state.editingNoteId = item?.id || null; state.dirty = false; state.editorMode = "write"; el.dialogTitle.textContent = item ? "Edit document" : "New document"; el.noteTitle.value = item?.title || ""; el.noteContent.value = item?.content || "";
  const draft = readDraft();
  if (draft && (draft.title !== el.noteTitle.value || draft.content !== el.noteContent.value) && window.confirm("Restore the local draft for this note?")) { el.noteTitle.value = draft.title || ""; el.noteContent.value = draft.content || ""; el.editorStatus.textContent = "Recovered local draft"; state.dirty = true; } else { el.editorStatus.textContent = "Drafts stay on this device until saved."; }
  setEditorMode("write"); updateEditorStatus(); el.dialog.showModal(); setTimeout(() => item ? el.noteContent.focus() : el.noteTitle.focus(), 0);
}

function openInfo(item) {
  el.infoTitle.textContent = item.title;
  el.infoType.textContent = item.type;
  el.infoSize.textContent = formatBytes(item.byteSize);
  el.infoMime.textContent = item.mediaType || "—";
  el.infoChecksum.textContent = item.checksum || "—";
  el.infoCreated.textContent = formatDate(item.createdAt);
  el.infoUpdated.textContent = formatDate(item.updatedAt);
  el.infoPreview.innerHTML = "";
  if (item.type === "video" && item.downloadUrl) { const vid = document.createElement("video"); vid.src = item.downloadUrl; vid.controls = true; vid.preload = "metadata"; vid.playsInline = true; vid.className = "info-media"; el.infoPreview.append(vid); }
  else if (item.type === "audio" && item.downloadUrl) { const aud = document.createElement("audio"); aud.src = item.downloadUrl; aud.controls = true; aud.preload = "metadata"; aud.className = "info-media"; el.infoPreview.append(aud); }
  else if (item.type === "image" && item.downloadUrl) { const img = document.createElement("img"); img.src = item.downloadUrl; img.alt = item.title; img.className = "info-media"; el.infoPreview.append(img); }
  el.infoDownload.href = item.downloadUrl || "#";
  el.infoDownload.style.display = item.downloadUrl ? "" : "none";
  el.infoDialog.showModal();
}

function uploadOne(file) { return new Promise((resolve, reject) => { const form = new FormData(), xhr = new XMLHttpRequest(); form.append("file", file); xhr.open("POST", "/api/v1/files"); xhr.upload.addEventListener("progress", event => { if (event.lengthComputable) el.progress.textContent = `${file.name} · ${Math.round(event.loaded / event.total * 100)}%`; }); xhr.addEventListener("load", () => xhr.status >= 200 && xhr.status < 300 ? resolve() : reject(new Error(`Upload failed (${xhr.status})`))); xhr.addEventListener("error", () => reject(new Error("The upload connection failed"))); xhr.send(form); }); }
async function uploadFiles(files) { const list = [...files]; if (!list.length) return; try { for (const file of list) await uploadOne(file); toast(`${list.length} file${list.length === 1 ? "" : "s"} stored on the host`); await loadItems(); } catch (error) { toast(error.message, true); } finally { el.progress.textContent = ""; el.fileInput.value = ""; } }

function insertMarkup(kind) {
  const ranges = {bold: ["**", "**", "bold text"], italic: ["*", "*", "italic text"], heading: ["### ", "", "heading"], link: ["[", "](https://)", "link text"], bullet: ["- ", "", "list item"], quote: ["> ", "", "quote"], code: ["`", "`", "code"]}; const range = ranges[kind]; if (!range) return; const start = el.noteContent.selectionStart, end = el.noteContent.selectionEnd, selected = el.noteContent.value.slice(start, end) || range[2]; el.noteContent.setRangeText(`${range[0]}${selected}${range[1]}`, start, end, "select"); el.noteContent.dispatchEvent(new Event("input", {bubbles: true})); el.noteContent.focus(); }

document.querySelector("#send-file").addEventListener("click", () => el.fileInput.click()); document.querySelector("#new-note").addEventListener("click", () => openNote()); document.querySelector("#new-task").addEventListener("click", () => openTask()); document.querySelector("#close-note").addEventListener("click", () => { if (!state.dirty || window.confirm("Discard unsaved changes?")) el.dialog.close(); }); document.querySelector("#close-task").addEventListener("click", () => el.taskDialog.close()); document.querySelector("#close-info").addEventListener("click", () => el.infoDialog.close()); el.taskForm.addEventListener("submit", saveTask); el.fileInput.addEventListener("change", () => uploadFiles(el.fileInput.files));
el.dropZone.addEventListener("dragover", event => { event.preventDefault(); el.dropZone.classList.add("dragging"); }); el.dropZone.addEventListener("dragleave", () => el.dropZone.classList.remove("dragging")); el.dropZone.addEventListener("drop", event => { event.preventDefault(); el.dropZone.classList.remove("dragging"); uploadFiles(event.dataTransfer.files); });
document.addEventListener("paste", event => { const items = event.clipboardData?.items; if (!items) return; const files = []; for (const item of items) { if (item.kind === "file") { const file = item.getAsFile(); if (file) files.push(file); } } if (files.length) uploadFiles(files); });
el.items.addEventListener("click", event => {
  const del = event.target.closest(".item-delete"); if (del) { event.stopPropagation(); trashItem(del.dataset.deleteId); return; }
  const row = event.target.closest(".item"); if (!row) return;
  const item = state.items.find(candidate => candidate.id === row.dataset.id); if (!item) return;
  if (item.type === "note") openNote(item);
  else if (event.target.closest(".item-media")) openInfo(item);
  else if (item.downloadUrl) window.location.assign(item.downloadUrl);
});
el.noteForm.addEventListener("submit", async event => { event.preventDefault(); if (!el.noteContent.value.trim()) { toast("Write something before saving", true); el.noteContent.focus(); return; } const payload = JSON.stringify({title: el.noteTitle.value.trim(), content: el.noteContent.value}), url = state.editingNoteId ? `/api/v1/notes/${state.editingNoteId}` : "/api/v1/notes"; el.saveNote.disabled = true; el.saveNote.textContent = "Saving…"; try { await api(url, {method: state.editingNoteId ? "PUT" : "POST", headers: {"Content-Type": "application/json"}, body: payload}); clearDraft(); state.dirty = false; el.dialog.close(); toast(state.editingNoteId ? "Document updated" : "Document created"); await loadItems(); } catch (error) { toast(error.message, true); } finally { el.saveNote.disabled = false; el.saveNote.textContent = "Save document"; } });
el.noteTitle.addEventListener("input", () => { state.dirty = true; updateEditorStatus(); saveDraft(); }); el.noteContent.addEventListener("input", () => { state.dirty = true; updateEditorStatus(); saveDraft(); }); document.querySelectorAll("[data-markup]").forEach(button => button.addEventListener("click", () => insertMarkup(button.dataset.markup))); document.querySelectorAll("[data-editor-mode]").forEach(tab => tab.addEventListener("click", () => setEditorMode(tab.dataset.editorMode)));
el.noteForm.addEventListener("keydown", event => { if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") { event.preventDefault(); el.noteForm.requestSubmit(); } if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "b") { event.preventDefault(); insertMarkup("bold"); } if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "i") { event.preventDefault(); insertMarkup("italic"); } if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k") { event.preventDefault(); insertMarkup("link"); } }); el.dialog.addEventListener("cancel", event => { if (state.dirty && !window.confirm("Discard unsaved changes?")) event.preventDefault(); });

let searchTimer; el.search.addEventListener("input", () => { clearTimeout(searchTimer); searchTimer = setTimeout(() => { state.query = el.search.value.trim(); loadItems(); }, 220); });
document.querySelectorAll(".nav-item").forEach(button => button.addEventListener("click", () => {
  document.querySelectorAll(".nav-item").forEach(item => item.classList.remove("active")); button.classList.add("active");
  state.filter = button.dataset.filter;
  const labels = {all: ["Everything", "Files and thoughts from every device, in one place."], file: ["Files", "Media and documents stored by the host."], note: ["Notes", "Thoughts that follow you across devices."], tasks: ["Tasks", "Small promises Sheila keeps on schedule."], recycle: ["Recycle bin", "Restore or permanently remove deleted items."]};
  document.querySelector("#view-title").textContent = labels[state.filter][0]; document.querySelector("#view-description").textContent = labels[state.filter][1];
  const itemView = !["tasks", "recycle"].includes(state.filter); el.items.hidden = !itemView; el.dropZone.hidden = !itemView; el.tasksPanel.hidden = state.filter !== "tasks"; el.recyclePanel.hidden = state.filter !== "recycle";
  if (state.filter === "tasks") loadTasks(); else if (state.filter === "recycle") loadRecycle(); else render();
}));
document.querySelectorAll(".sub-filter").forEach(button => button.addEventListener("click", () => {
  document.querySelectorAll(".sub-filter").forEach(s => s.classList.remove("active")); button.classList.add("active");
  state.subfilter = button.dataset.subfilter; render();
}));

document.addEventListener("keydown", event => {
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "n") { event.preventDefault(); openNote(); }
  if (event.key === "Escape") { if (el.infoDialog.open) el.infoDialog.close(); }
});

function connectEvents() { const protocol = location.protocol === "https:" ? "wss:" : "ws:", socket = new WebSocket(`${protocol}//${location.host}/api/v1/events`); socket.addEventListener("open", () => { el.connection.textContent = "Host online"; el.connection.classList.add("online"); }); socket.addEventListener("message", event => { try { const message = JSON.parse(event.data); if (message.type === "items.changed") loadItems(); else if (message.type === "tasks.changed") loadTasks(); else if (message.type === "recycle.changed") loadRecycle(); else if (message.type === "reminder.due") toast(`Reminder: ${message.title}`); } catch {} }); socket.addEventListener("close", () => { el.connection.textContent = "Reconnecting"; el.connection.classList.remove("online"); setTimeout(connectEvents, 2000); }); }
api("/api/v1/system").then(system => { el.storage.textContent = `${formatBytes(system.storage.availableBytes)} free`; }).catch(() => { el.storage.textContent = "Storage unavailable"; }); loadItems(); loadTasks(); loadRecycle(); connectEvents();
