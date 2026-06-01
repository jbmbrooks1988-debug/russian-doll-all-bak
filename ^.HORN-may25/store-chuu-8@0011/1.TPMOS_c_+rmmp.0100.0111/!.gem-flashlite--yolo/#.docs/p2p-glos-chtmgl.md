# Blueprint: P2P-GLOS Multi-Media Chat Forum
**Status:** SPECIFICATION
**Reference:** `glos-tpmos-dev.md` (Phase 3)

---

## 1. Objective
Develop a sovereign GL-OS application that provides real-time, peer-to-peer multi-media communication, utilizing `p2p-net` for transport and CHTMGL for rich content rendering.

## 2. Layout Structure (`layouts/main.chtmgl`)

```html
<window title="P2P-GLOS Forum" sovereign="true">
    <panel id="sidebar" width="25%">
        <ul id="peer-list">
            ${active_peers}
        </ul>
    </panel>

    <panel id="chat-area" width="75%">
        <chat-stream id="main-feed">
            <!-- Rendered by gl_renderer via history.txt -->
            ${chat_history}
        </chat-stream>

        <panel id="input-tray" height="50px">
            <cli_io id="chat_input" label="Type message..." />
            <button label="Send" onClick="OP:SEND_P2P_MSG" />
            <button label="+" onClick="OP:ATTACH_MEDIA" />
        </panel>
    </panel>
</window>
```

## 3. Core Components

### 3.1 P2P Transport Layer (`p2p-net`)
- **Standard IPC:** The chat feed uses `history.txt` and `state.txt` for text message persistence and synchronization.
- **High-Performance IPC:** Large media assets and `<media-embed>` updates utilize the SHM layer and `SHAPE` protocol for efficient binary/buffer handling.
- **Protocol:** Messages are transmitted as JSON objects containing:
    ```json
    {
      "sender": "peer_id",
      "timestamp": 123456789,
      "type": "TEXT | MEDIA",
      "content": "...",
      "media_hash": "sha256..."
    }
    ```

### 3.2 CHTMGL Custom Tags
- **`<chat-stream>`:** A specialized container that implements auto-scrolling to bottom and handles high-frequency text updates.
- **`<media-embed>`:** 
    - **Usage:** `<media-embed type="image" src="cache/hash.png" />`.
    - **Logic:** The renderer checks `cache/` for the hash. If missing, the manager triggers a P2P fetch request.

### 3.3 Security & Sovereignty
- **Hash Verification:** All incoming media MUST match the broadcasted hash before the `<media-embed>` is rendered.
- **Sanitization:** String content is stripped of malicious escape codes before being published to `gui_state.txt`.

## 4. Technical Roadmap
1.  **Transport Test:** Verify `p2p-net` can send/receive raw strings between two GL-OS instances.
2.  **Markup Proof:** Implement a static `chat-stream` in `chtmgl-alpha` with dummy images.
3.  **Dynamic Integration:** Connect the P2P receiver to the `${chat_history}` variable update loop.

## 5. Verification KPIs
- **Message Latency:** P2P delivery < 200ms on local networks.
- **Asset Integrity:** 100% hash verification success rate for media.
- **UI Responsiveness:** The UI must not block during large media P2P transfers (Async Fetch).
