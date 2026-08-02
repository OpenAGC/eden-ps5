# Eden PS5 Agent Instructions

- Use Codex subagents for independent implementation, investigation, and review work when parallelism will make the task faster.
- Do not use ACP Router unless the user explicitly requests it.
- Before sending or launching any new PS5 ELF, run the pinned cleanup ELF and independently verify that no exact `eboot.bin` process remains.
