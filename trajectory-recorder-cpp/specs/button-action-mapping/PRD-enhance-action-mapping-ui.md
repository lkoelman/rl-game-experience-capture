The action mapping workflow in src\ActionMappingWorkflow.cpp is awkward: I want to improve the user experience by changing the workflow and UI.

- the currently pressed gamepad button should be shown in real-time
    - we should press Space on the keyboard to confirm the mapping to the currently pressed button
- instead of pressing Enter to confirm, 'n' to skip, we should use the keyboard arrow keys:
    - right arrow key to confirm or skip (i.e. it should confirm the bindings, or just skip if none were created)
    - left arrow to go back to the previous action
- there should be a "stop" keyboard shortcut
    - exit the mapping workflow and save al the actions mapped until now
    - this should be the keyboard "Enter" key
- the TUI should have two columns:
    - the first column should be the current dialog
    - the second column should be a list of all the actions loaded for the class
        - we should use the FTXUI 'Menu' element for the list
            - example 1: https://github.com/ArthurSonzogni/FTXUI/blob/main/examples/component/menu.cpp
            - example 2: https://github.com/ArthurSonzogni/FTXUI/blob/main/examples/component/menu2.cpp
            - example 3: https://github.com/ArthurSonzogni/FTXUI/blob/main/examples/component/menu_in_frame.cpp
        - the current action should be highlighted in the list
        - this should make it easier to jump between actions usting the left and right arrow keys
    - for example, we could use a `ResizableSplitRight` component for the columns
- there should be a new argument that accepts an existing mapping config, and picks up from the existing bindings (i.e. captures the remaning/unmapped actions for the classs)


Documentation of FTXUI components can be found at https://arthursonzogni.github.io/FTXUI/group__component.html