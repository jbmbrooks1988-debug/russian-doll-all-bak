================================================================================
P2P MULTI-NETWORK ARCHITECTURE DECISION
================================================================================
Date: April 2, 2026
Status: BAKED INTO PHASE 1 FROM THE START

================================================================================
THE REQUIREMENT (From User)
================================================================================

> "i think we should be able to do multiple network connections from the same 
> terminal (if desired) using subnetting etc. i just want our network to have 
> that flexibility baked in from the start, instead of trying to add it later, 
> since we dont know what we may want for what reason, in the future, get it?"

**Translation:** Don't paint ourselves into a corner. Build multi-network
support from DAY 1, not as an afterthought.

================================================================================
WHY THIS MATTERS
================================================================================

**Wrong Way (Single-Network First):**
1. Build everything assuming ONE network connection
2. Hardcode single socket, single peer list, single chat log
3. Later realize: "Wait, what if I want general + auction active?"
4. Try to refactor: socket handling, peer tracking, message routing
5. Break everything, spend weeks fixing

**Right Way (Multi-Network From Start):**
1. Each subnet = independent state (config, peers, chat logs)
2. Socket handling designed for multiple from day 1
3. Message routing includes subnet tag
4. Can join/leave subnets independently
5. Future features just work: auction chat, trade rooms, game lobbies

================================================================================
HOW IT WORKS (Architecture)
================================================================================

**Per-Subnet State:**
```
pieces/network/subnets/
├── general.txt      ← Config for "general" subnet
├── chat.txt         ← Config for "chat" subnet
├── auction.txt      ← Config for "auction" subnet
└── trade.txt        ← Config for "trade" subnet

pieces/chat/
├── general_msgs.txt ← Chat log for "general"
├── chat_msgs.txt    ← Chat log for "chat"
├── auction_msgs.txt ← Chat log for "auction"
└── trade_msgs.txt   ← Chat log for "trade"

peers.txt (unified, but tagged):
├── abc123...|127.0.0.1|8000|general|active
├── def456...|127.0.0.1|8001|chat|active
├── ghi789...|127.0.0.1|8000|general|active
└── jkl012...|127.0.0.1|8001|chat|active
          ↑
          SUBNET TAG - same peer can exist on multiple subnets!
```

**Key Design Decisions:**

1. **Subnet Parameter on Every Op:**
   - `OP p2p::broadcast_join "chat"`
   - `OP p2p::send_message "<wallet>" "Hi" "auction"`
   - `OP p2p::broadcast_leave "general"`

2. **Independent Sockets:**
   - Each subnet = separate UDP broadcast socket
   - Each subnet = separate TCP listener thread
   - Each subnet = separate peer connections

3. **Message Routing:**
   - Message header includes subnet: `MSG:<wallet>:<subnet>:<ts>:<msg>`
   - Receiver routes to correct chat log based on subnet tag
   - Can listen to multiple subnets simultaneously

4. **State Isolation:**
   - Leave "chat" subnet → only affects chat state
   - "general" and "auction" stay connected
   - No cascade failures

================================================================================
USE CASES (Why This Matters)
================================================================================

**Today:**
- User joins "chat" subnet to talk
- User joins "general" for announcements

**Near Future:**
- User joins "auction" to bid on items
- User joins "trade" to propose swaps
- User joins "games" for game-specific chat

**Far Future:**
- User joins "dao" for governance voting
- User joins "market" for price discovery
- User joins "support" for help tickets
- Cross-subnet messaging (bridge chat ↔ auction)

**Without Multi-Network Design:**
- Would need to disconnect from chat to join auction
- Or hacky "switch network" that loses state
- Or complete rewrite of networking layer

**With Multi-Network Design:**
- Just: `OP p2p::broadcast_join "auction"`
- Already connected to chat, general, AND auction
- Messages arrive on all three
- Leave one without affecting others

================================================================================
IMPLEMENTATION IMPACT
================================================================================

**What Changes in Phase 1:**

1. configure_subnet:
   - Creates per-subnet config file
   - NOT global config

2. broadcast_join:
   - Takes subnet parameter
   - Joins THAT subnet only

3. list_peers:
   - Takes subnet filter
   - Shows peers on THAT subnet (or "all")

4. send_message:
   - Takes subnet parameter
   - Routes via correct socket
   - Logs to correct chat file

5. broadcast_leave:
   - Takes subnet parameter
   - Leaves THAT subnet only

**Code Complexity:**
- Slightly more complex than single-network
- BUT: Future features require ZERO refactoring
- Trade-off: +20% initial dev time, -80% future refactor time

================================================================================
EXAMPLE: Future Feature (Auction House)
================================================================================

**With Multi-Network Already Built In:**

Week 5: Implement auction ops
```bash
# User already on "general" and "chat"
# Just add "auction" subnet
OP p2p::configure_subnet "auction" "hybrid" "continuous" 8002
OP p2p::broadcast_join "auction"

# Now connected to 3 subnets simultaneously
# Auction messages arrive on "auction" subnet
# Chat messages still arrive on "chat" subnet
# General announcements on "general" subnet
```

**Without Multi-Network:**
Week 5: Realize we need it
Week 6-8: Refactor entire networking layer
Week 9: Test and fix broken stuff
Week 10: Finally implement auction

================================================================================
CONCLUSION
================================================================================

This is the right call. Baking in multi-network support from day 1:

✅ Prevents future refactoring nightmares
✅ Enables features we haven't thought of yet
✅ Minimal additional complexity in Phase 1
✅ Maximum flexibility for unknown future use cases

As you said: "we dont know what we may want for what reason, in the future"

This design ensures we're ready for whatever that is.

================================================================================
END OF ARCHITECTURE DECISION RECORD
================================================================================
