================================================================================
P2P-NET PHASE 1: NETWORK INFRASTRUCTURE + CHAT
================================================================================
Date: April 2, 2026
Priority: CRITICAL - START HERE
Status: READY FOR IMPLEMENTATION

================================================================================
ARCHITECTURAL PRINCIPLE: MULTI-NETWORK FROM DAY 1
================================================================================

**CRITICAL:** A single terminal can connect to MULTIPLE networks simultaneously.

Why:
- Don't know what future use cases will be
- Subnets may need to communicate (chat ↔ auction ↔ trade)
- User might want general + games + auction active at once
- Easier to build multi-network support NOW than add later

**Implementation:**
- Each subnet = independent connection state
- peers.txt tracks subnet per peer
- Messages routed by subnet
- Can join/leave subnets independently

================================================================================
WHAT WE'RE BUILDING (8 Ops Total)
================================================================================

**Goal:** Basic P2P connectivity + chat messaging WITH MULTI-NETWORK SUPPORT

**Ops to Implement:**
1. p2p::configure_subnet    - Configure network (creates subnet config)
2. p2p::broadcast_join      - Join network via UDP broadcast
3. p2p::connect_peer        - TCP connection to peer
4. p2p::disconnect_peer     - Disconnect from peer
5. p2p::list_peers          - Show connected peers (by subnet or all)
6. p2p::ping_peer           - Keepalive (auto every 30s)
7. p2p::send_message        - Chat message to peer ← KEY FEATURE
8. p2p::broadcast_leave     - Graceful disconnect (from specific subnet)

**Test Scenario:**
One terminal connects to BOTH "general" and "chat" subnets simultaneously,
sending/receiving messages on each independently.

================================================================================
PROJECT STRUCTURE (What to Create)
================================================================================

projects/p2p-net/
├── project.pdl              # Create this
├── layouts/
│   └── p2p-net.chtpm        # Create this (dashboard UI)
├── manager/
│   └── +x/p2p_manager.+x    # Create this (network manager)
├── ops/
│   ├── +x/                  # Compile 8 ops here
│   │   ├── configure_subnet.+x
│   │   ├── broadcast_join.+x
│   │   ├── connect_peer.+x
│   │   ├── disconnect_peer.+x
│   │   ├── list_peers.+x
│   │   ├── ping_peer.+x
│   │   ├── send_message.+x
│   │   └── broadcast_leave.+x
│   └── ops_manifest.txt     # Create this (register ops)
├── pieces/
│   ├── network/
│   │   ├── peers.txt              # All connected peers
│   │   └── subnets/               # Per-subnet configs
│   │       ├── general.txt        # general subnet config
│   │       ├── chat.txt           # chat subnet config
│   │       ├── auction.txt        # auction subnet config
│   │       └── trade.txt          # trade subnet config
│   └── chat/
│       ├── general_msgs.txt       # Chat log per subnet
│       ├── chat_msgs.txt
│       ├── auction_msgs.txt
│       └── trade_msgs.txt
└── src/                     # Or put .c files in ops/

**KEY:** Each subnet has independent state files.

================================================================================
OP 1: p2p::configure_subnet
================================================================================

Location: projects/p2p-net/ops/configure_subnet.c
Purpose: Create/update subnet configuration (per subnet!)

Args:
  - subnet_name: "general", "chat", "auction", "trade", "games", etc.
  - network_mode: "broadcast", "ring", "hybrid"
  - connection_mode: "continuous", "on_demand"
  - port: 8000-8010 (per subnet)

What It Does:
  1. Creates/updates pieces/network/subnets/<subnet_name>.txt
  2. Sets network mode for THIS subnet only
  3. Starts/stops listener daemon for THIS subnet (continuous mode)
  4. Other subnets UNAFFECTED (can run simultaneously)

subnet-config.txt Format (per subnet file):
```
subnet_name=chat
network_mode=broadcast
connection_mode=continuous
port=8001
max_peers=50
ping_interval=30
timeout=90
active=1
```

Test:
```bash
# Configure multiple subnets independently
OP p2p::configure_subnet "general" "broadcast" "continuous" 8000
OP p2p::configure_subnet "chat" "broadcast" "continuous" 8001
OP p2p::configure_subnet "auction" "hybrid" "on_demand" 8002

# Check: All three configs exist
cat projects/p2p-net/pieces/network/subnets/general.txt
cat projects/p2p-net/pieces/network/subnets/chat.txt
cat projects/p2p-net/pieces/network/subnets/auction.txt
```

Implementation Notes:
- Each subnet = separate config file
- Each subnet = separate UDP/TCP sockets
- Each subnet = independent listener thread
- Can join/leave subnets independently

================================================================================
OP 2: p2p::broadcast_join
================================================================================

Location: projects/p2p-net/ops/broadcast_join.c
Purpose: Join a specific subnet via UDP broadcast

Args:
  - subnet_name: (optional, default from config)

What It Does:
  1. Reads subnet config (pieces/network/subnets/<subnet>.txt)
  2. Creates UDP broadcast socket for THIS subnet
  3. Sends JOIN message: "JOIN:<wallet>:<ip>:<port>:<subnet>"
  4. Listens for responses (peer lists)
  5. Adds peers to peers.txt WITH SUBNET TAG
  6. Starts ping timer for THIS subnet (every 30s)

Broadcast Format:
  - Address: 255.255.255.255:<subnet_port>
  - Message: "JOIN:<wallet_address>:<your_ip>:<your_port>:<subnet_name>"
  - Response: "PEERS:<subnet>:<count>|<peer1>|<peer2>|..."

peers.txt Format (WITH SUBNET):
```
wallet_address|ip|port|subnet|last_seen|status
abc123...|127.0.0.1|8000|general|1775106600|active
def456...|127.0.0.1|8001|chat|1775106600|active
ghi789...|127.0.0.1|8000|general|1775106600|active
jkl012...|127.0.0.1|8001|chat|1775106600|active
```

Test:
```bash
# Join multiple subnets from same terminal
OP p2p::broadcast_join "general"
OP p2p::broadcast_join "chat"

# Check: peers.txt shows both subnets
cat projects/p2p-net/pieces/network/peers.txt
```

Implementation Notes:
- UDP broadcast on subnet port (from config)
- Non-blocking socket
- Timeout after 5 seconds
- Parse peer list with subnet tag
- Same peer can exist in multiple subnets!

================================================================================
OP 3: p2p::connect_peer
================================================================================

Location: projects/p2p-net/ops/connect_peer.c
Purpose: Establish TCP connection to specific peer

Args:
  - peer_ip: "127.0.0.1" or "192.168.1.100"
  - peer_port: 8000-8010
  - subnet: optional (default from config)

What It Does:
  1. Creates TCP socket
  2. Connects to peer
  3. Sends handshake: "Hello:<wallet>:<port>"
  4. Waits for response: "Ack:<peer_wallet>:<peer_port>"
  5. Adds to peers.txt with status=connected

Handshake Protocol:
```
Client → Server: "Hello:<my_wallet>:<my_port>"
Server → Client: "Ack:<server_wallet>:<server_port>"
```

Test:
```bash
OP p2p::connect_peer "127.0.0.1" 8001 "chat"
```

Implementation Notes:
- Blocking TCP connection
- 5-second timeout for handshake
- Validate response format
- Update peers.txt

================================================================================
OP 4: p2p::disconnect_peer
================================================================================

Location: projects/p2p-net/ops/disconnect_peer.c
Purpose: Gracefully disconnect from peer

Args:
  - peer_ip or peer_wallet
  - peer_port (optional)

What It Does:
  1. Sends LEAVE message
  2. Closes TCP socket
  3. Updates peers.txt (status=disconnected or remove)

Test:
```bash
OP p2p::disconnect_peer "127.0.0.1" 8001
```

================================================================================
OP 5: p2p::list_peers
================================================================================

Location: projects/p2p-net/ops/list_peers.c
Purpose: Display connected peers

Args:
  - subnet: "all", "chat", "auction", etc.

Output Format:
```
Connected Peers (subnet: chat)
────────────────────────────────────────────────────────
Wallet          | IP          | Port | Status  | Last Seen
────────────────────────────────────────────────────────
abc123...       | 127.0.0.1   | 8000 | active  | 0s ago
def456...       | 127.0.0.1   | 8001 | active  | 5s ago
────────────────────────────────────────────────────────
Total: 2 peers
```

Test:
```bash
OP p2p::list_peers "all"
OP p2p::list_peers "chat"
```

================================================================================
OP 6: p2p::ping_peer
================================================================================

Location: projects/p2p-net/ops/ping_peer.c
Purpose: Send keepalive ping

Args:
  - peer_ip
  - peer_port

What It Does:
  1. Sends "PING" message via TCP
  2. Waits for "PONG" response
  3. Updates last_seen timestamp
  4. Removes peer if no response after 90s

Auto-Ping:
- Continuous mode: auto-ping every 30s
- Managed by p2p_manager.c (background thread)

Test:
```bash
OP p2p::ping_peer "127.0.0.1" 8001
```

================================================================================
OP 7: p2p::send_message (CHAT - KEY FEATURE)
================================================================================

Location: projects/p2p-net/ops/send_message.c
Purpose: Send chat message to peer ON SPECIFIC SUBNET

Args:
  - peer_wallet: recipient address
  - message: text message
  - subnet: (optional) which subnet to send on (default: last active)

What It Does:
  1. Finds peer in peers.txt ON SPECIFIED SUBNET
  2. Sends via TCP: "MSG:<sender_wallet>:<subnet>:<timestamp>:<message>"
  3. Logs to chat/<subnet>_msgs.txt
  4. Returns success/failure

Message Format (TCP):
```
MSG:abc123...:chat:1775106600:Hello! How are you?
MSG:def456...:auction:1775106605:Bidding on item #123
```

chat/<subnet>_msgs.txt Format (per subnet):
```
[1775106600] abc123... → def456... (chat): Hello!
[1775106605] def456... → abc123... (auction): Bid 50 TPM
[1775106610] abc123... → ghi789... (general): Anyone online?
```

Test (Multi-Network Scenario):
```bash
# Terminal 1: Join both subnets
OP p2p::configure_subnet "general" "broadcast" "continuous" 8000
OP p2p::configure_subnet "chat" "broadcast" "continuous" 8001
OP p2p::broadcast_join "general"
OP p2p::broadcast_join "chat"

# Terminal 2: Also join both
OP p2p::configure_subnet "general" "broadcast" "continuous" 8000
OP p2p::configure_subnet "chat" "broadcast" "continuous" 8001
OP p2p::broadcast_join "general"
OP p2p::broadcast_join "chat"

# Terminal 1: Send on different subnets
OP p2p::send_message "<Terminal2_Wallet>" "Hello on general!" "general"
OP p2p::send_message "<Terminal2_Wallet>" "Hello on chat!" "chat"

# Check: Separate chat logs
cat projects/p2p-net/pieces/chat/general_msgs.txt
cat projects/p2p-net/pieces/chat/chat_msgs.txt
```

Implementation Notes:
- TCP connection must exist on specified subnet
- Route message via correct subnet socket
- Log to subnet-specific chat file
- Max message length: 1024 chars
- Subnet tag in message header for routing

================================================================================
OP 8: p2p::broadcast_leave
================================================================================

Location: projects/p2p-net/ops/broadcast_leave.c
Purpose: Leave a specific subnet (graceful disconnect)

Args:
  - subnet_name: (optional) which subnet to leave (default: all)

What It Does:
  1. Sends LEAVE broadcast on specified subnet
  2. Closes TCP connections for THIS subnet only
  3. Updates peers.txt (status=disconnected for subnet peers)
  4. Stops listener thread for THIS subnet (continuous mode)
  5. OTHER SUBNETS UNAFFECTED

Test:
```bash
# Leave only chat subnet, stay on general
OP p2p::broadcast_leave "chat"

# Leave ALL subnets
OP p2p::broadcast_leave "all"
```

================================================================================
MANAGER: p2p_manager.c
================================================================================

Location: projects/p2p-net/manager/+x/p2p_manager.+x
Purpose: Background network manager

Responsibilities:
1. Poll input (like other modules)
2. Auto-ping peers every 30s (continuous mode)
3. Handle incoming messages (TCP listener thread)
4. Update peers.txt
5. Log to master_ledger.txt

Main Loop:
```c
while (!g_shutdown) {
    if (!is_active_layout()) { usleep(100000); continue; }
    
    // Check for new messages from peers
    process_incoming_messages();
    
    // Auto-ping if continuous mode
    if (connection_mode == CONTINUOUS) {
        auto_ping_peers();
    }
    
    // Poll user input
    poll_history_file();
    
    usleep(16667); // 60 FPS
}
```

================================================================================
UI LAYOUT: p2p-net.chtpm
================================================================================

Location: projects/p2p-net/layouts/p2p-net.chtpm

```
╔═══════════════════════════════════════════════════════════╗
║           P 2 P   N E T   D A S H B O A R D              ║
║                                                           ║
║  Wallet: ${wallet_address}                                ║
║  Balance: ${wallet_balance} TPM                           ║
║  Status: ${network_status}                                ║
║                                                           ║
╠═══════════════════════════════════════════════════════════╣
║  CONNECTED PEERS (${peer_count})                          ║
║  ─────────────────────────────────────────────────────    ║
║  [ ] 1. ${peer1_wallet} (${peer1_ip})                     ║
║  [ ] 2. ${peer2_wallet} (${peer2_ip})                     ║
║                                                           ║
╠═══════════════════════════════════════════════════════════╣
║  CHAT LOG                                                 ║
║  ─────────────────────────────────────────────────────    ║
║  ${chat_messages}                                         ║
║                                                           ║
╠═══════════════════════════════════════════════════════════╣
║  ACTIONS                                                  ║
║  [1] Configure Network   [5] Send Message                ║
║  [2] Join Network        [6] Chat Log                    ║
║  [3] List Peers          [7] Leave Network               ║
║  [4] Connect Peer                                       ║
║                                                           ║
╠═══════════════════════════════════════════════════════════╣
║  [RESP]: ${p2p_response}                                  ║
║  [KEY]: ${last_key}                                       ║
╚═══════════════════════════════════════════════════════════╝
```

================================================================================
IMPLEMENTATION CHECKLIST
================================================================================

Week 1: Network Foundation
──────────────────────────
[ ] 1. Create project structure
[ ] 2. Create project.pdl
[ ] 3. Create ops_manifest.txt
[ ] 4. Implement p2p::configure_subnet
[ ] 5. Implement p2p::list_peers
[ ] 6. Test: Config loads, peers list empty

Week 2: Connectivity
────────────────────
[ ] 7. Implement p2p::broadcast_join
[ ] 8. Implement p2p::connect_peer
[ ] 9. Implement p2p::disconnect_peer
[ ] 10. Implement p2p::ping_peer
[ ] 11. Test: Two peers can connect

Week 3: Chat
────────────
[ ] 12. Implement p2p::send_message
[ ] 13. Implement p2p::broadcast_leave
[ ] 14. Create p2p_manager.c
[ ] 15. Create p2p-net.chtpm
[ ] 16. Test: Two terminals can chat

Week 4: Polish
──────────────
[ ] 17. Add error handling
[ ] 18. Add logging to master_ledger.txt
[ ] 19. Test: All 8 ops work together
[ ] 20. Run: ./fondu --install p2p-net

================================================================================
TEST SCENARIOS
================================================================================

Scenario 1: Single Subnet (Basic Test)
──────────────────────────────────────
```bash
cd /path/to/TPMOS
./run_chtpm.sh

# Launch p2p-net
OP p2p::configure_subnet "chat" "broadcast" "continuous" 8001
OP p2p::broadcast_join "chat"
OP p2p::list_peers "chat"
# Expected: Shows no peers (yet)
```

Scenario 2: Two Peers Chatting (Basic)
──────────────────────────────────────
Terminal 1:
```bash
OP p2p::configure_subnet "chat" "broadcast" "continuous" 8001
OP p2p::broadcast_join "chat"
OP p2p::list_peers "chat"  # Note your wallet address
```

Terminal 2:
```bash
OP p2p::configure_subnet "chat" "broadcast" "continuous" 8001
OP p2p::broadcast_join "chat"
OP p2p::list_peers "chat"  # Should see Terminal 1
OP p2p::send_message "<Terminal1_Wallet>" "Hello!" "chat"
```

Terminal 1:
```bash
# Check chat log
cat projects/p2p-net/pieces/chat/chat_msgs.txt
# Should see: [timestamp] <Wallet2> → <Wallet1> (chat): Hello!
```

Scenario 3: MULTI-NETWORK (One Terminal, Multiple Subnets) ⭐
────────────────────────────────────────────────────────────
**This is the power of baking in multi-network from the start!**

Terminal 1:
```bash
# Configure 3 subnets
OP p2p::configure_subnet "general" "broadcast" "continuous" 8000
OP p2p::configure_subnet "chat" "broadcast" "continuous" 8001
OP p2p::configure_subnet "auction" "hybrid" "on_demand" 8002

# Join all 3
OP p2p::broadcast_join "general"
OP p2p::broadcast_join "chat"
OP p2p::broadcast_join "auction"

# List peers on each
OP p2p::list_peers "general"
OP p2p::list_peers "chat"
OP p2p::list_peers "auction"
```

Terminal 2 (same setup):
```bash
OP p2p::configure_subnet "general" "broadcast" "continuous" 8000
OP p2p::configure_subnet "chat" "broadcast" "continuous" 8001
OP p2p::configure_subnet "auction" "hybrid" "on_demand" 8002
OP p2p::broadcast_join "general"
OP p2p::broadcast_join "chat"
OP p2p::broadcast_join "auction"
```

Terminal 1 (send on different subnets):
```bash
# General chat
OP p2p::send_message "<Terminal2_Wallet>" "Welcome to general!" "general"

# Private chat
OP p2p::send_message "<Terminal2_Wallet>" "Hey, wanna trade?" "chat"

# Auction bid
OP p2p::send_message "<Terminal2_Wallet>" "I bid 100 TPM!" "auction"
```

Check logs:
```bash
# Three separate chat logs
cat projects/p2p-net/pieces/chat/general_msgs.txt
cat projects/p2p-net/pieces/chat/chat_msgs.txt
cat projects/p2p-net/pieces/chat/auction_msgs.txt
```

Scenario 4: Leave One Subnet, Stay on Others
────────────────────────────────────────────
```bash
# Leave chat, stay on general and auction
OP p2p::broadcast_leave "chat"

# Verify: Still connected to general and auction
OP p2p::list_peers "general"  # Should show peers
OP p2p::list_peers "auction"  # Should show peers
OP p2p::list_peers "chat"     # Should show none (left)
```

================================================================================
NEXT PHASES (After Phase 1 Complete)
================================================================================

Phase 2: Wallet & Mining
  - p2p::wallet_login
  - p2p::mine_block
  - p2p::get_balance
  - p2p::send_coins

Phase 3: Items & NFTs
  - p2p::create_item
  - p2p::list_inventory
  - p2p::create_auction

See: 2do.txt for full roadmap

================================================================================
END OF PHASE 1 PLAN
================================================================================
