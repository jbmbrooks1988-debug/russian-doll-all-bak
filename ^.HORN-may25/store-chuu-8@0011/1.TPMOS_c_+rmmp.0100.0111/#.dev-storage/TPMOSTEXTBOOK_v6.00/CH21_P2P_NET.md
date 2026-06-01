# 🕸️ Chapter 21: P2P-NET & TSOTS-ONLINE: Elegant Decentralization
In v6.00, TPMOS is no longer a solitary island. Through **P2P-NET**, every instance of the OS can connect to a global, sovereign mesh network. 🌐🤝

---

## 🕸️ The Mesh Topology
P2P-NET does not use a central server. It uses a **Decentralized Mesh** based on the `known_hosts.pdl` directory.

### Node Discovery
1.  **Bootstrapping:** A new node reads `known_hosts.pdl` to find its first peer.
2.  **Gossip:** Peers exchange lists of other active nodes.
3.  **Ring Formation:** Nodes organize into a logical ring for efficient message passing.

---

## 🗳️ Consensus & Leader Election
To maintain a single "Source of Truth" across multiple machines, P2P-NET uses a **Consensus Algorithm**.
*   **Leader Election:** The node with the lowest IP address (or highest uptime) is elected as the "Ring Master."
*   **Conflict Resolution:** If two nodes update the same Piece simultaneously, the Ring Master decides which update is written to the global `chat_ledger.txt`.

---

## 🔑 TSOTS-ONLINE: Unified Authentication
**TSOTS** (The Sovereign Online) is the identity layer built on top of P2P-NET.
*   **Unified Auth:** Your profile (`pieces/profiles/user/state.txt`) is cryptographically signed.
*   **Cross-Node Persistence:** When you "log in" to a different TPMOS node, your Pieces, Gold, and History follow you via the P2P sync engine.

---

## 💻 Code Example: P2P Host Entry
```pdl
SECTION      | KEY                | VALUE
----------------------------------------
IP_LIST      | node_0             | 192.168.1.10
IP_LIST      | node_1             | 10.0.0.238
META         | protocol           | TCP_PORT_8000
```

---

## 🏛️ Scholar's Corner: The "Incorruptible Ledger"
The `chat_ledger.txt` is the heart of TSOTS. During a test in 2026, a malicious node tried to inject a fake transaction ("Give Node_X 1,000,000 Gold"). Because the other 10 nodes in the mesh checked the transaction against their own ledgers and found a mismatch, the fake update was automatically rejected. This **"Incorruptible Ledger"** is the foundation of trust in the TPMOS ecosystem. 🛡️📜

---

## 📝 Study Questions
1.  How does a node discover other peers in P2P-NET?
2.  What is the role of the "Ring Master" in consensus?
3.  Explain how TSOTS-ONLINE handles user identity without a central server.
4.  **Scenario:** Three nodes are connected. Node A loses internet. What happens to the mesh?

---
[Return to Index](INDEX.md)
