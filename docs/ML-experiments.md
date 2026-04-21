## Text Conditioning

- encode gamepad mapping in task description
    - use special tokens for gamepad buttons and for skills
        - skill encoding: specific skills or skill types/categories

## Action Encoding

- binned movement vectors

- skill type encoding
    - encode the combination of raned/meelee, directed-single-target, directed-AoE, delayed-AoE, delayed-physical-damage, delayed-spell-damage, delayed effect
    - categories to encode
        - directed, directed AoE / multi-target, undirected AoE
        - physical damage, spell damage, spell effect
        - immediate effect, delayed effect, damage-over-time
        - mana/energy use
        - cooldown

- raw gamepad inputs
    - with gamepad mapping encoded in text prompt / task description (see above)