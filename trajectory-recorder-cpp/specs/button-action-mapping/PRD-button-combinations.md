The action mapping workflow and action mapping profile (config file) currently only support single buttons. Add support for button combinations. For example, an in-game action can be triggered by simultaneously holding the left shoulder button and the south button.

Requirements
- set the maximum simultaneous button presses as a constant, with a default of 2
    - we should be able to override this using a commandline argument
- an axis + threshold as one of the simultaneous buttons pressed
    - for example, holding the left trigger (axis) + the south button is a valid button combination
    - the thresholds for registering axes as button presses should be part of the mapping profile (config file), as a separate entry (not per-action). The threshold should default to 0.5
- when starting the action mapping CLI, the first dialog should ask the user to start the mapping workflow (current behaviour) or to configure axis thresholds (for registering them as button presses, see above)
    - this dialog should be a menu where you can select the option from a list menu