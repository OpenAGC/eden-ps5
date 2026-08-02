# Eden PS5 Agent Instructions

- Use Codex subagents for independent implementation, investigation, and review work when parallelism will make the task faster.
- Use the `gpt-5.6-terra` model with high reasoning for subagents when it is available; otherwise use the closest available GPT-5.6 model with high reasoning.
- Do not use ACP Router unless the user explicitly requests it.
- Before sending or launching any new PS5 ELF, run the pinned cleanup ELF and independently verify that no exact `eboot.bin` process remains.
