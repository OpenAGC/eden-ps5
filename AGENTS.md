# Eden PS5 Agent Instructions

- Use Codex subagents for independent implementation, investigation, and review work when parallelism will make the task faster.
- Use the `gpt-5.6-terra` model with high reasoning for subagents when it is available; otherwise use the closest available GPT-5.6 model with high reasoning.
- Do not use ACP Router unless the user explicitly requests it.
- Before sending or launching any new PS5 ELF, run the pinned cleanup ELF and independently verify that no exact `eboot.bin` process remains.
- Never send or launch the rejected fixed-address diagnostic ELF with SHA-256 `b3122a9a6137a99985651f84a424577139ad1676322650fa1c972957d2a8d2a1`. Its forced 4 GiB `MAP_FIXED` mapping can overlap live process mappings and caused a PS5 kernel panic. Do not use broad fixed-address mappings for placement probes; use OS-chosen VA-only reservations and fail closed when placement is inconclusive.
- Treat direct `/dev/gc` and installed `libSceAgcDriver` GPU use as mutually exclusive for the entire PS5 boot. The active Eden qualification uses only the direct backend. Any installed-driver oracle requires a fresh boot and must not fall back to, precede, or follow direct `/dev/gc` work before the next reboot.
