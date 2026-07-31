AGENT PROGRESS REPORT — j31 (2026-07-31)
=========================================
Working agent: muchi-pal-agent (045.muchi-pal-agent🤖️+1)
Report: session 1 of the "run-the-agent + build-the-team" pass.
Companions: dev-agent-convo/00.session-001🌌.md, 01.assessment💼️.md,
02.LAN-nodes🌐.md, 03.IQABOD-test-report🔬.md, 04.arch-strategy🧠.md,
05.genius-idea💡.md, agent-onbording.txt.

PART 1 — WHAT WAS DONE THIS SESSION (verified, with evidence)
--------------------------------------------------------------
1. Agent harness baseline: 5/5 scenario harnesses PASS
   (proof/ under the agent). list_dir, H1 write+run python, H2 edit-book,
   iqabod-chat (PITFALL-60 fix confirmed), model-remember dual-boot.
2. Code-gen probe (on LAN nodes, NOT this box — compute constraint):
   - gemma3:270m → cannot write code on ANY node (chat/tool-router only)
   - stable-code (Mac) → syntax fail
   - llama3:latest (Mac) → correct, playable game (deployed+verified as
     ~/games/number_game.py on the Mac)
   - qwen2.5-coder:7b is on the Linux box; proven valid code locally earlier.
3. LAN nodes online + documented:
   - MAC lfs.master@10.0.0.144 (ollama LAN-open :11434) — heavy lifting
   - LINUX jb@10.0.0.187 (I exposed ollama host=0.0.0.0, pulled gemma3:270m)
     — reliable, slower, gemma-only
4. IQABOD tested on the Linux node:
   - pipeline works: vocab_only 0.02s, train 10 epochs 0.11s
   - pre-trained 32-token curriculum: semi-coherent word order + <UNK> leaks
   - loss descends but plateaus at random floor (2.6225→2.6211, ln14≈2.64)
     → LR (1e-5) too low is the prime suspect — mechanism OK, optimizer weak
   - binaries are plain libm/libc; easy to compile on any node
5. Docs created: agent-onbording.txt + dev-agent-convo/00-05 (session,
   assessment, LAN nodes, IQABOD report, arch strategy, genius-idea).

PART 2 — LOCKED DECISIONS (user, 2026-07-31)
---------------------------------------------
D1 Storage: this box = tiny manifest (curricula.lst + reports) ONLY.
   Nodes hold heavy data: ~/iqabod-store/ on Linux and Mac.
   Pointer, not payload (like pointers.pdl). Zip-and-ship safe.
D2 iqabod = BANK OF SMALL EXPERTS + ROUTER (per-curriculum weights, never
   merged, never all-in-one-place). NOT full MoE. Multiple labeled scale
   systems per machine; prove toy-scale loop first.
D3 Genius idea = ON-DEMAND DISTILLATION with gap-driven teacher prompting:
   gap report (<UNK>/weak spots) → prompt llama3/gemma "fill these gaps" →
   retrain (0.1s) → expanded brain. Buildable, no new theory.
D4 Supervisor = reusable OPS + a harness that calls them (house pattern).
D5 Teacher = llama3 primary, gemma3:270m fallback.
D6 First goal = reasonable CONVERSATION (grade curriculums); story writing +
   basic pal coding = documented adjacent goals.
D7 Agent tool pipeline (write/edit/read/run/search) is the shared toolset;
   gemma+iqabod are provider_kinds behind one router.

PART 3 — WHAT I'M STARTING NOW (supervisor v1)
----------------------------------------------
A. Storage layout on nodes (do first — everything stores there):
   Linux 10.0.0.187: ~/iqabod-store/<curriculum>/{corpus.txt,vocab,weights}
   Mac 10.0.0.144:   ~/iqabod-store/ (for big-scale runs later)
B. curricula.lst manifest on THIS box (local, tiny):
   name|topic|rung|expert_location|weights_size|last_report|verdict
C. New ops (ops/, self-contained C, house style, reusable):
   - train_step.c     queue entry → teacher generate → vocab_only+train on
                      the target node → eval numbers → verdict string
   - eval_curriculum.c held-out prompts → loss + <UNK>% + optional
                      llama-as-judge → report row (gap report for D3)
   - list_curricula.c read curricula.lst manifest
D. Harness: %.harnesses/iqabod-loop/ (or agent scripts/) — reads
   curriculum-queue.txt, calls the ops in order, appends training-report.txt,
   prints one-line summary, sleeps (set-and-forget capable).
E. First queue: conversation curriculum (grade1: simple chat vocab).

PART 4 — NEXT AFTER SUPERVISOR v1 (in order)
--------------------------------------------
1. Gap-report teacher prompting (D3) — same ops, smarter prompt.
2. Router op: intent/topic → pick curriculum expert.
3. Bridge harness: gemma routes + iqabod expert generates → file lands.
4. Behavior-tree decision engine (replace hardcoded gemma_strategy weights).
5. escalate_to_llama op (management-review rung).

STATUS: all green. Supervisor v1 build is next.
