# P2P-NET Planning Documents

This directory contains all P2P-NET implementation plans, architecture decisions, and roadmaps.

## Contents

### Phase 1: Network Infrastructure + Chat
- **P2P-PHASE-1-NETWORK-AND-CHAT.md** - Implementation guide for 8 ops (network + chat)
- **P2P-MULTI-NETWORK-ARCHITECTURE.md** - Architecture decision record (multi-network support)

### Strategic Overview
- **../p2p_plan.txt** - High-level P2P strategy (return to #.plans/ root)

### Full Specification
- **../../#.main/p2p-ops.txt** - Complete ops specification (1491 lines)

## Implementation Status

**Phase 1: Network Infrastructure + Chat** - READY TO IMPLEMENT
- 8 ops: configure_subnet, broadcast_join, connect_peer, disconnect_peer, list_peers, ping_peer, send_message, broadcast_leave
- Multi-network support baked in from start
- Test scenario: Two terminals chatting on multiple subnets

**Future Phases** - See 2do.txt for full roadmap:
- Phase 2: Wallet & Mining
- Phase 3: Items & NFTs
- Phase 4: Recursive Wallets
- Phase 5: Rental System
- Phase 6: Smart Contracts
- Phase 7: Exchange & Custom Coins

## Where to Start

1. Read: **P2P-PHASE-1-NETWORK-AND-CHAT.md** (implementation guide)
2. Review: **P2P-MULTI-NETWORK-ARCHITECTURE.md** (why multi-network matters)
3. Reference: **../../#.main/p2p-ops.txt** (full op specs)
4. Track: **../../2do.txt** (development checklist)

## Directory Structure

```
#.plans/#.p2p/
├── README.md                      ← This file
├── P2P-PHASE-1-NETWORK-AND-CHAT.md ← Start here!
└── P2P-MULTI-NETWORK-ARCHITECTURE.md ← Architecture decision
```

---

*"Bake in flexibility from the start, instead of trying to add it later."*
